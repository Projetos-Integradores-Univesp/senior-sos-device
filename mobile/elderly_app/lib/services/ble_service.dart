import 'dart:async';
import 'dart:developer' as dev;
import 'dart:convert';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

// ============================================================================
// BleService — ElderGuard Provisioning
// ============================================================================

const String _kServiceUuid = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const String _kCharSsid    = 'beb5483e-36e1-4688-b7f5-ea07361b26a0';
const String _kCharPass    = 'beb5483e-36e1-4688-b7f5-ea07361b26a1';
const String _kCharId      = 'beb5483e-36e1-4688-b7f5-ea07361b26a2';
const String _kCharStatus  = 'beb5483e-36e1-4688-b7f5-ea07361b26a3';
const String _kCharCmd     = 'beb5483e-36e1-4688-b7f5-ea07361b26a4';

const String kElderGuardDeviceName = 'ElderGuard';

class ProvisionResult {
  final bool    success;
  final String? deviceId;
  final String? error;
  const ProvisionResult.ok(String id)   : success = true,  deviceId = id, error = null;
  const ProvisionResult.err(String msg) : success = false, deviceId = null, error = msg;
}

enum BleState { idle, scanning, connecting, connected, provisioning, done, error }

class BleService {
  final _stateCtrl   = StreamController<BleState>.broadcast();
  final _devicesCtrl = StreamController<List<BluetoothDevice>>.broadcast();

  // Stream de todos os dispositivos vistos (sem filtro) — para debug na UI
  final _allDevicesCtrl = StreamController<List<ScanResult>>.broadcast();

  Stream<BleState>              get stateStream      => _stateCtrl.stream;
  Stream<List<BluetoothDevice>> get devicesStream    => _devicesCtrl.stream;
  Stream<List<ScanResult>>      get allDevicesStream => _allDevicesCtrl.stream;

  BleState         _state = BleState.idle;
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _charSsid, _charPass, _charId, _charStatus, _charCmd;

  final List<BluetoothDevice> _found       = [];
  final List<ScanResult>      _allResults  = [];   // todos os devices vistos

  // --------------------------------------------------------------------------
  // scan()
  //
  // Ordem correta:
  //   1. startScan() PRIMEIRO — inicia o hardware de scan
  //   2. listen() DEPOIS — garante que nenhum resultado é perdido por
  //      race condition entre subscribe e início do scan
  // --------------------------------------------------------------------------
  Future<void> scan({int timeoutSec = 25}) async {
    _found.clear();
    _allResults.clear();
    _emit(BleState.scanning);

    if (await FlutterBluePlus.adapterState.first != BluetoothAdapterState.on) {
      _emit(BleState.error);
      throw Exception('Bluetooth desligado.');
    }

    // Parar scan anterior
    if (await FlutterBluePlus.isScanning.first) {
      await FlutterBluePlus.stopScan();
      await Future.delayed(const Duration(milliseconds: 400));
    }

    // ── PASSO 1: iniciar scan ANTES de criar o listener ────────────────────
    // Sem filtros: qualquer filtro de hardware no Android pode descartar
    // o dispositivo se nome e UUID chegarem em pacotes separados (ESP32).
    await FlutterBluePlus.startScan(
      timeout: Duration(seconds: timeoutSec),
    );

    // ── PASSO 2: listener DEPOIS do startScan ──────────────────────────────
    // onScanResults: emite List<ScanResult> com os resultados de cada ciclo.
    // Acumulamos manualmente para não perder nenhum device.
    final sub = FlutterBluePlus.onScanResults.listen((results) {
      bool changed = false;
      for (final r in results) {
        final name  = r.device.platformName;
        final adv   = r.advertisementData.advName;
        final uuids = r.advertisementData.serviceUuids
            .map((u) => u.toString().toLowerCase()).toList();
        final rssi  = r.rssi;

        // Log completo de cada dispositivo encontrado
        dev.log(
          '[BLE SCAN] name="$name" advName="$adv" '
          'rssi=$rssi uuids=$uuids id=${r.device.remoteId}',
          name: 'ElderGuard',
        );

        // Acumular todos (para debug na UI)
        final existsAll = _allResults
            .any((x) => x.device.remoteId == r.device.remoteId);
        if (!existsAll) {
          _allResults.add(r);
          _allDevicesCtrl.add(List.unmodifiable(_allResults));
        }

        // Filtrar ElderGuard
        final match = _isElderGuard(r);
        dev.log(
          '[BLE FILTER] "$name" / "$adv" → ${match ? "MATCH ✓" : "descartado"}',
          name: 'ElderGuard',
        );

        if (!match) continue;
        if (_found.any((d) => d.remoteId == r.device.remoteId)) continue;
        _found.add(r.device);
        changed = true;
      }
      if (changed) _devicesCtrl.add(List.unmodifiable(_found));
    });

    // Aguardar fim do scan
    await FlutterBluePlus.isScanning.where((s) => !s).first;
    sub.cancel();

    if (_found.isEmpty) _emit(BleState.idle);
  }

  // --------------------------------------------------------------------------
  // connect()
  // --------------------------------------------------------------------------
  Future<void> connect(BluetoothDevice device) async {
    _emit(BleState.connecting);
    try {
      await device.connect(
        timeout: const Duration(seconds: 15),
        autoConnect: false,
      );
      _connectedDevice = device;

      final services = await device.discoverServices();
      final svc = services.firstWhere(
        (s) => _norm(s.serviceUuid.toString()) == _norm(_kServiceUuid),
        orElse: () => throw Exception('Serviço ElderGuard não encontrado.'),
      );

      _charSsid   = _findChar(svc, _kCharSsid);
      _charPass   = _findChar(svc, _kCharPass);
      _charId     = _findChar(svc, _kCharId);
      _charStatus = _findChar(svc, _kCharStatus);
      _charCmd    = _findChar(svc, _kCharCmd);

      _emit(BleState.connected);
    } catch (e) {
      await _doDisconnect();
      _emit(BleState.error);
      rethrow;
    }
  }

  // --------------------------------------------------------------------------
  // provision()
  // --------------------------------------------------------------------------
  Future<ProvisionResult> provision({
    required String ssid,
    required String password,
    required String deviceId,
    Duration timeout = const Duration(seconds: 30),
  }) async {
    if (_charSsid == null || _charStatus == null) {
      return const ProvisionResult.err('Dispositivo não conectado.');
    }
    _emit(BleState.provisioning);

    final completer = Completer<ProvisionResult>();
    StreamSubscription? notifySub;

    try {
      await _charStatus!.setNotifyValue(true);
      await Future.delayed(const Duration(milliseconds: 300));

      notifySub = _charStatus!.lastValueStream.listen((value) {
        if (value.isEmpty) return;
        final json = _parseJson(utf8.decode(value));
        if (json == null || completer.isCompleted) return;
        if (json['provisioned'] == true) {
          completer.complete(
              ProvisionResult.ok(json['id']?.toString() ?? deviceId));
        } else if (json.containsKey('error')) {
          completer.complete(ProvisionResult.err(json['error'].toString()));
        }
      });

      await _charSsid!.write(utf8.encode(ssid), withoutResponse: false);
      await Future.delayed(const Duration(milliseconds: 300));
      await _charPass!.write(utf8.encode(password), withoutResponse: false);
      await Future.delayed(const Duration(milliseconds: 300));
      await _charId!.write(utf8.encode(deviceId), withoutResponse: false);

      final result = await completer.future.timeout(timeout,
          onTimeout: () => const ProvisionResult.err(
              'Tempo esgotado aguardando confirmação.'));

      _emit(result.success ? BleState.done : BleState.error);
      return result;
    } catch (e) {
      _emit(BleState.error);
      return ProvisionResult.err(e.toString());
    } finally {
      await notifySub?.cancel();
      await _charStatus?.setNotifyValue(false).catchError((_) {});
    }
  }

  Future<bool>    sendCommand(String cmd) async {
    if (_charCmd == null) return false;
    try {
      await _charCmd!.write(utf8.encode(cmd), withoutResponse: false);
      return true;
    } catch (_) { return false; }
  }

  Future<String?> readCurrentId() async {
    if (_charId == null) return null;
    try {
      final v = await _charId!.read();
      return v.isEmpty ? null : utf8.decode(v);
    } catch (_) { return null; }
  }

  Future<void> disconnect() async {
    await _doDisconnect();
    _emit(BleState.idle);
  }

  void dispose() {
    _stateCtrl.close();
    _devicesCtrl.close();
    _allDevicesCtrl.close();
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  bool _isElderGuard(ScanResult r) {
    // Critério 1: nome exato (platformName ou advName)
    if (r.device.platformName == kElderGuardDeviceName) return true;
    if (r.advertisementData.advName == kElderGuardDeviceName) return true;

    // Critério 2: UUID de serviço completo normalizado
    final target = _norm(_kServiceUuid);
    if (r.advertisementData.serviceUuids
        .any((u) => _norm(u.toString()) == target)) return true;

    // Critério 3: UUID parcial (primeiros 8 chars) — cobre casos onde o UUID
    // chega truncado ou com formatação diferente
    if (r.advertisementData.serviceUuids
        .any((u) => u.toString().toLowerCase().startsWith('4fafc201'))) return true;

    // Critério 4: nome case-insensitive — cobre "elderguard" ou "ELDERGUARD"
    if (r.device.platformName.toLowerCase() == kElderGuardDeviceName.toLowerCase()) return true;
    if (r.advertisementData.advName.toLowerCase() == kElderGuardDeviceName.toLowerCase()) return true;

    return false;
  }

  String _norm(String u) => u.toLowerCase().replaceAll('-', '');

  BluetoothCharacteristic _findChar(BluetoothService svc, String uuid) =>
      svc.characteristics.firstWhere(
        (c) => _norm(c.characteristicUuid.toString()) == _norm(uuid),
        orElse: () => throw Exception('Característica $uuid não encontrada.'),
      );

  Future<void> _doDisconnect() async {
    try { await _connectedDevice?.disconnect(); } catch (_) {}
    _connectedDevice = null;
    _charSsid = _charPass = _charId = _charStatus = _charCmd = null;
  }

  void _emit(BleState s) {
    _state = s;
    if (!_stateCtrl.isClosed) _stateCtrl.add(s);
  }

  Map<String, dynamic>? _parseJson(String s) {
    try { return jsonDecode(s) as Map<String, dynamic>; }
    catch (_) { return null; }
  }

  BleState              get currentState  => _state;
  bool                  get isConnected   => _connectedDevice != null;
  List<BluetoothDevice> get foundDevices  => List.unmodifiable(_found);
  List<ScanResult>      get allResults    => List.unmodifiable(_allResults);
}
