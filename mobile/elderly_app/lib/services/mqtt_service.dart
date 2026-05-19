import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'mqtt_config.dart';

// ============================================================================
// MqttService — subscribe nos tópicos do ElderGuard e emite alertas
//
// Tópicos monitorados (por device ID):
//   devices/{id}/fall            → alerta de queda
//   devices/{id}/button-pressed  → botão de pânico
//
// Uso:
//   final svc = MqttService();
//   await svc.connect(config, deviceIds: ['watch-a1b2c3', 'watch-xyz']);
//   svc.alertStream.listen((alert) { ... });
//   await svc.disconnect();
// ============================================================================

// Estado da conexão MQTT
enum MqttConnState { disconnected, connecting, connected, error }

// Evento de alerta recebido via MQTT
class MqttAlert {
  final String deviceId;
  final String type;        // 'fall' | 'button-pressed'
  final String topic;
  final Map<String, dynamic> payload;
  final DateTime receivedAt;

  const MqttAlert({
    required this.deviceId,
    required this.type,
    required this.topic,
    required this.payload,
    required this.receivedAt,
  });

  // Converte para o modelo Alert já existente no app
  String get title {
    switch (type) {
      case 'fall':           return '⚠️ Queda detectada';
      case 'button-pressed': return '🆘 Botão de pânico';
      default:               return 'Alerta do aparelho';
    }
  }

  String get message {
    final batt = payload['batt'];
    final accel = payload['peak_accel_ms2'];
    switch (type) {
      case 'fall':
        return [
          'Queda confirmada.',
          if (accel != null) 'Pico: ${(accel as num).toStringAsFixed(1)} m/s²',
          if (batt != null)  'Bateria: $batt%',
        ].join(' ');
      case 'button-pressed':
        return [
          'Botão de pânico acionado.',
          if (batt != null) 'Bateria: $batt%',
        ].join(' ');
      default:
        return payload.toString();
    }
  }
}

class MqttService {
  MqttServerClient? _client;
  MqttConfig?       _config;

  // Streams públicos
  final _connStateCtrl = StreamController<MqttConnState>.broadcast();
  final _alertCtrl     = StreamController<MqttAlert>.broadcast();
  final _logCtrl       = StreamController<String>.broadcast();

  Stream<MqttConnState> get connStateStream => _connStateCtrl.stream;
  Stream<MqttAlert>     get alertStream     => _alertCtrl.stream;
  Stream<String>        get logStream        => _logCtrl.stream;

  MqttConnState _connState = MqttConnState.disconnected;
  MqttConnState get connState => _connState;

  final List<String> _subscribedDeviceIds = [];

  // --------------------------------------------------------------------------
  // connect() — estabelece conexão e subscreve tópicos dos dispositivos
  // --------------------------------------------------------------------------
  Future<void> connect(MqttConfig config, {List<String> deviceIds = const []}) async {
    if (_connState == MqttConnState.connected) await disconnect();

    _config = config;
    _emitConn(MqttConnState.connecting);
    _log('Conectando a ${config.host}:${config.port} '
         '(TLS: ${config.useTls ? "sim" : "não"})…');

    final clientId = 'elderguard-app-${DateTime.now().millisecondsSinceEpoch}';

    _client = MqttServerClient.withPort(config.host, clientId, config.port);
    _client!.keepAlivePeriod = 60;
    _client!.autoReconnect   = true;
    _client!.resubscribeOnAutoReconnect = true;
    _client!.logging(on: false);

    if (config.useTls) {
      _client!.secure = true;
      // Aceitar certificados autoassinados em dev; em produção remover esta linha
      _client!.onBadCertificate = (_) => true;
      _client!.securityContext  = SecurityContext.defaultContext;
    }

    _client!.onConnected    = _onConnected;
    _client!.onDisconnected = _onDisconnected;
    _client!.onAutoReconnect = () => _log('Reconectando…');

    final connMessage = MqttConnectMessage()
        .withClientIdentifier(clientId)
        .authenticateAs(config.username, config.password)
        .withWillQos(MqttQos.atLeastOnce)
        .startClean();
    _client!.connectionMessage = connMessage;

    try {
      final status = await _client!.connect();
      if (status?.state != MqttConnectionState.connected) {
        throw Exception('Estado inesperado: ${status?.state}');
      }
    } catch (e) {
      _log('Erro de conexão: $e');
      _client?.disconnect();
      _client = null;
      _emitConn(MqttConnState.error);
      rethrow;
    }

    // Subscrever tópicos dos dispositivos já conhecidos
    for (final id in deviceIds) {
      _subscribeDevice(id);
    }

    // Escutar mensagens recebidas
    _client!.updates!.listen(_onMessage);
  }

  // --------------------------------------------------------------------------
  // subscribeDevice() — adiciona um device ID à escuta em tempo real
  // --------------------------------------------------------------------------
  void subscribeDevice(String deviceId) {
    if (_subscribedDeviceIds.contains(deviceId)) return;
    _subscribeDevice(deviceId);
  }

  void _subscribeDevice(String deviceId) {
    if (_client == null ||
        _client!.connectionStatus?.state != MqttConnectionState.connected) return;

    final topics = [
      'devices/$deviceId/fall',
      'devices/$deviceId/button-pressed',
    ];
    for (final topic in topics) {
      _client!.subscribe(topic, MqttQos.atLeastOnce);
      _log('Subscrito: $topic');
    }
    if (!_subscribedDeviceIds.contains(deviceId)) {
      _subscribedDeviceIds.add(deviceId);
    }
  }

  // --------------------------------------------------------------------------
  // unsubscribeDevice() — remove escuta de um dispositivo
  // --------------------------------------------------------------------------
  void unsubscribeDevice(String deviceId) {
    if (_client == null) return;
    for (final suffix in ['fall', 'button-pressed']) {
      _client!.unsubscribe('devices/$deviceId/$suffix');
      _log('Unsubscrito: devices/$deviceId/$suffix');
    }
    _subscribedDeviceIds.remove(deviceId);
  }

  // --------------------------------------------------------------------------
  // disconnect()
  // --------------------------------------------------------------------------
  Future<void> disconnect() async {
    _client?.disconnect();
    _client = null;
    _subscribedDeviceIds.clear();
    _emitConn(MqttConnState.disconnected);
    _log('Desconectado.');
  }

  // --------------------------------------------------------------------------
  // dispose()
  // --------------------------------------------------------------------------
  void dispose() {
    _client?.disconnect();
    _connStateCtrl.close();
    _alertCtrl.close();
    _logCtrl.close();
  }

  // ── Handlers internos ────────────────────────────────────────────────────

  void _onConnected() {
    _emitConn(MqttConnState.connected);
    _log('Conectado. Aguardando alertas…');
    // Re-subscrever todos os dispositivos após reconexão
    for (final id in List.of(_subscribedDeviceIds)) {
      _subscribeDevice(id);
    }
  }

  void _onDisconnected() {
    // autoReconnect cuida da reconexão; só notificamos a UI
    if (_connState != MqttConnState.disconnected) {
      _emitConn(MqttConnState.disconnected);
      _log('Conexão perdida.');
    }
  }

  void _onMessage(List<MqttReceivedMessage<MqttMessage>> events) {
    for (final event in events) {
      final topic   = event.topic;
      final pubMsg  = event.payload as MqttPublishMessage;
      final raw     = MqttPublishPayload.bytesToStringAsString(
          pubMsg.payload.message);

      _log('← $topic: $raw');

      // Identificar device ID e tipo a partir do tópico
      // Formato: devices/{id}/{type}
      final parts = topic.split('/');
      if (parts.length < 3 || parts[0] != 'devices') continue;

      final deviceId  = parts[1];
      final alertType = parts.sublist(2).join('/'); // 'fall' ou 'button-pressed'

      Map<String, dynamic> payload = {};
      try {
        payload = jsonDecode(raw) as Map<String, dynamic>;
      } catch (_) {
        payload = {'raw': raw};
      }

      final alert = MqttAlert(
        deviceId:   deviceId,
        type:       alertType,
        topic:      topic,
        payload:    payload,
        receivedAt: DateTime.now(),
      );

      if (!_alertCtrl.isClosed) _alertCtrl.add(alert);
    }
  }

  void _emitConn(MqttConnState state) {
    _connState = state;
    if (!_connStateCtrl.isClosed) _connStateCtrl.add(state);
  }

  void _log(String msg) {
    if (!_logCtrl.isClosed) _logCtrl.add(msg);
  }

  List<String> get subscribedDeviceIds => List.unmodifiable(_subscribedDeviceIds);
}
