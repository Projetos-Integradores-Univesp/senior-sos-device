import 'dart:async';
import 'dart:convert';
import 'package:flutter_foreground_task/flutter_foreground_task.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'mqtt_config.dart';

// ============================================================================
// ElderGuard Foreground Service — API flutter_foreground_task ^8.17.0
// ============================================================================

// ── Configuração (chamada no main()) ──────────────────────────────────────────
void initForegroundTask() {
  FlutterForegroundTask.init(
    androidNotificationOptions: AndroidNotificationOptions(
      channelId: 'elderguard_service',
      channelName: 'ElderGuard Monitoramento',
      channelDescription: 'Mantém o monitoramento ativo em segundo plano.',
      channelImportance: NotificationChannelImportance.LOW,
      priority: NotificationPriority.LOW,
    ),
    iosNotificationOptions: const IOSNotificationOptions(
      showNotification: true,
      playSound: false,
    ),
    foregroundTaskOptions: ForegroundTaskOptions(
      eventAction: ForegroundTaskEventAction.repeat(5000),
      autoRunOnBoot: true,
      allowWakeLock: true,
      allowWifiLock: true,
    ),
  );
}

// ── Iniciar serviço ───────────────────────────────────────────────────────────
Future<void> startForegroundService(
    MqttConfig config, List<String> deviceIds,
    {Map<String, String> deviceNames = const {}}) async {
  await FlutterForegroundTask.requestNotificationPermission();

  if (await FlutterForegroundTask.isRunningService) {
    _updateServiceData(config, deviceIds);
    return;
  }

  await FlutterForegroundTask.startService(
    serviceId: 1001,
    notificationTitle: 'ElderGuard ativo',
    notificationText: 'Monitorando ${deviceIds.length} dispositivo(s)',
    callback: startMqttTaskCallback,
  );

  // Enviar configuração inicial logo após iniciar
  await Future.delayed(const Duration(milliseconds: 500));
  _updateServiceData(config, deviceIds, deviceNames);
}

void _updateServiceData(MqttConfig config, List<String> deviceIds,
    [Map<String, String> deviceNames = const {}]) {
  FlutterForegroundTask.sendDataToTask(jsonEncode({
    'action':      'update',
    'host':        config.host,
    'port':        config.port,
    'username':    config.username,
    'password':    config.password,
    'useTls':      config.useTls,
    'deviceIds':   deviceIds,
    'deviceNames': deviceNames,
  }));
}

Future<void> stopForegroundService() =>
    FlutterForegroundTask.stopService();

// Top-level — obrigatório para o isolate
@pragma('vm:entry-point')
void startMqttTaskCallback() {
  FlutterForegroundTask.setTaskHandler(MqttTaskHandler());
}

// ── TaskHandler — roda no isolate do ForegroundService ───────────────────────
class MqttTaskHandler extends TaskHandler {
  MqttServerClient? _client;
  List<String>      _deviceIds   = [];
  Map<String,String> _deviceNames = {};
  MqttConfig        _config    = MqttConfig.defaults;
  final _notif                 = FlutterLocalNotificationsPlugin();
  bool  _notifInit             = false;
  int   _notifId               = 200;

  // ── API 8.17.0: onStart(DateTime, TaskStarter) ───────────────────────────
  @override
  Future<void> onStart(DateTime timestamp, TaskStarter starter) async {
    _config    = await MqttConfig.load();
    final prefs = await SharedPreferences.getInstance();
    _deviceIds   = prefs.getStringList('monitored_device_ids') ?? [];
    final rawNames = prefs.getString('monitored_device_names');
    if (rawNames != null) {
      final decoded = jsonDecode(rawNames) as Map<String, dynamic>;
      _deviceNames = decoded.map((k, v) => MapEntry(k, v.toString()));
    }
    await _initNotif();
    await _connect();
  }

  // ── API 8.17.0: onRepeatEvent(DateTime) ──────────────────────────────────
  @override
  void onRepeatEvent(DateTime timestamp) {
    if (_client == null ||
        _client!.connectionStatus?.state != MqttConnectionState.connected) {
      _connect();
    }
  }

  // ── API 8.17.0: onDestroy(DateTime) ──────────────────────────────────────
  @override
  Future<void> onDestroy(DateTime timestamp) async {
    _client?.disconnect();
  }

  @override
  void onReceiveData(Object data) {
    try {
      final map = jsonDecode(data as String) as Map<String, dynamic>;
      if (map['action'] == 'update') {
        final newConfig = MqttConfig(
          host:     map['host']     as String,
          port:     map['port']     as int,
          username: map['username'] as String,
          password: map['password'] as String,
          useTls:   map['useTls']   as bool,
        );
        final newIds = List<String>.from(map['deviceIds'] as List);
        final rawNames = map['deviceNames'];
        final newNames = rawNames is Map
            ? rawNames.map((k, v) => MapEntry(k.toString(), v.toString()))
            : <String, String>{};

        final configChanged = newConfig.host     != _config.host     ||
                              newConfig.port     != _config.port     ||
                              newConfig.username != _config.username ||
                              newConfig.password != _config.password;
        final idsChanged = !_listEquals(newIds, _deviceIds);

        _config      = newConfig;
        _deviceIds   = newIds;
        _deviceNames = newNames;

        SharedPreferences.getInstance().then((p) {
          p.setStringList('monitored_device_ids', _deviceIds);
          p.setString('monitored_device_names',
              jsonEncode(_deviceNames));
        });

        if (configChanged || idsChanged) {
          _client?.disconnect();
          _client = null;
          _connect();
        }

        FlutterForegroundTask.updateService(
          notificationTitle: 'ElderGuard ativo',
          notificationText:  'Monitorando ${_deviceIds.length} dispositivo(s)',
        );
      }
    } catch (_) {}
  }

  @override
  void onNotificationPressed() => FlutterForegroundTask.launchApp('/');

  // ── MQTT ──────────────────────────────────────────────────────────────────
  Future<void> _connect() async {
    if (_deviceIds.isEmpty) return;
    final id = 'eg-fg-${DateTime.now().millisecondsSinceEpoch}';
    _client  = MqttServerClient.withPort(_config.host, id, _config.port);
    _client!.keepAlivePeriod            = 60;
    _client!.autoReconnect              = true;
    _client!.resubscribeOnAutoReconnect = true;
    _client!.logging(on: false);
    if (_config.useTls) {
      _client!.secure           = true;
      _client!.onBadCertificate = (_) => true;
    }
    _client!.connectionMessage = MqttConnectMessage()
        .withClientIdentifier(id)
        .authenticateAs(_config.username, _config.password)
        .startClean();
    try {
      await _client!.connect();
    } catch (_) { _client = null; return; }

    if (_client!.connectionStatus?.state != MqttConnectionState.connected) {
      _client = null; return;
    }
    for (final did in _deviceIds) {
      _client!.subscribe('devices/$did/fall',           MqttQos.atLeastOnce);
      _client!.subscribe('devices/$did/button-pressed', MqttQos.atLeastOnce);
    }
    _client!.updates!.listen(_onMsg);
  }

  void _onMsg(List<MqttReceivedMessage<MqttMessage>> events) {
    for (final e in events) {
      final parts = e.topic.split('/');
      if (parts.length < 3) continue;
      _notify(parts[1], parts[2]);
    }
  }

  // ── Notificações ──────────────────────────────────────────────────────────
  Future<void> _initNotif() async {
    if (_notifInit) return;
    await _notif.initialize(const InitializationSettings(
      android: AndroidInitializationSettings('@mipmap/ic_launcher'),
      iOS:     DarwinInitializationSettings(),
    ));
    final ap = _notif.resolvePlatformSpecificImplementation<
        AndroidFlutterLocalNotificationsPlugin>();
    await ap?.createNotificationChannel(const AndroidNotificationChannel(
      'elderguard_fall', 'Quedas',
      importance: Importance.max, playSound: true, enableVibration: true,
    ));
    await ap?.createNotificationChannel(const AndroidNotificationChannel(
      'elderguard_button', 'Botao de Panico',
      importance: Importance.max, playSound: true, enableVibration: true,
    ));
    _notifInit = true;
  }

  Future<void> _notify(String deviceId, String alertType) async {
    if (!_notifInit) await _initNotif();
    final isFall   = alertType == 'fall';
    final channel  = isFall ? 'elderguard_fall' : 'elderguard_button';
    final chName   = isFall ? 'Quedas' : 'Botao de Panico';
    // Usar apelido cadastrado se disponível
    final nickname = _deviceNames[deviceId];
    final label    = nickname != null
        ? '$nickname (ID: $deviceId)'
        : 'ID: $deviceId';
    await _notif.show(
      _notifId++,
      isFall ? 'Queda detectada' : 'Botao de panico',
      label,
      NotificationDetails(
        android: AndroidNotificationDetails(
          channel, chName,
          importance: Importance.max, priority: Priority.high,
        ),
        iOS: const DarwinNotificationDetails(
            presentAlert: true, presentSound: true),
      ),
    );
  }

  bool _listEquals(List<String> a, List<String> b) {
    if (a.length != b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }
}
