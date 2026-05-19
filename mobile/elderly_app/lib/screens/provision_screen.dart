import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:wifi_scan/wifi_scan.dart';
import '../services/ble_service.dart';

// ============================================================================
// ProvisionScreen — Tela de configuração via BLE
//
// Substitui o AddEditDeviceDialog para dispositivos NOVOS.
// Fluxo: Scan → Selecionar dispositivo → Preencher dados → Provisionar
//
// Retorna o deviceId confirmado pelo dispositivo ao fechar com Navigator.pop().
// ============================================================================
class ProvisionScreen extends StatefulWidget {
  final String deviceNickname;
  /// ID inteiro retornado pelo backend — enviado ao ESP32 via BLE
  final int    deviceId;

  const ProvisionScreen({
    super.key,
    required this.deviceNickname,
    required this.deviceId,
  });

  @override
  State<ProvisionScreen> createState() => _ProvisionScreenState();
}

class _ProvisionScreenState extends State<ProvisionScreen> {
  final _ble = BleService();
  final _ssidController     = TextEditingController();
  final _passController     = TextEditingController();
  final _formKey            = GlobalKey<FormState>();

  BleState _state     = BleState.idle;
  String   _statusMsg = '';
  BluetoothDevice? _selectedDevice;
  bool _obscurePass   = true;
  List<BluetoothDevice> _found        = [];
  List<WiFiAccessPoint> _wifiNetworks = [];
  bool                  _loadingWifi  = false;
  String?               _hotspotSsid;   // SSID do hotspot do próprio celular, se ativo
  List<ScanResult>      _allResults = [];
  final List<String>    _scanLog   = [];

  @override
  void initState() {
    super.initState();
    _ble.stateStream.listen((s) {
      if (!mounted) return;
      setState(() {
        _state = s;
        _statusMsg = _stateLabel(s);
      });
    });
    _ble.devicesStream.listen((devices) {
      if (!mounted) return;
      setState(() => _found = devices);
    });
    _ble.allDevicesStream.listen((all) {
      if (!mounted) return;
      setState(() {
        _allResults = all;
        // Atualizar log com último device adicionado
        if (all.isNotEmpty) {
          final r = all.last;
          final name = r.device.platformName;
          final adv  = r.advertisementData.advName;
          final uuids = r.advertisementData.serviceUuids
              .map((u) => u.toString().substring(0, 8)).join(',');
          _scanLog.add('name="$name" adv="$adv" uuids=[$uuids] rssi=${r.rssi}');
          if (_scanLog.length > 30) _scanLog.removeAt(0);
        }
      });
    });
    // Iniciar scan BLE e WiFi ao abrir a tela
    _startScan();
    _scanWifi();
    _detectHotspot();
  }

  @override
  void dispose() {
    _ble.disconnect().then((_) => _ble.dispose());
    _ssidController.dispose();
    _passController.dispose();
    super.dispose();
  }

  String _stateLabel(BleState s) {
    switch (s) {
      case BleState.idle:         return 'Aguardando';
      case BleState.scanning:     return 'Buscando dispositivos ElderGuard…';
      case BleState.connecting:   return 'Conectando…';
      case BleState.connected:    return 'Conectado! Preencha os dados abaixo.';
      case BleState.provisioning: return 'Enviando configurações…';
      case BleState.done:         return 'Dispositivo configurado com sucesso!';
      case BleState.error:        return 'Erro — tente novamente';
    }
  }

  Future<void> _startScan() async {
    setState(() {
      _found = [];
      _selectedDevice = null;
      _allResults = [];
      _scanLog.clear();
    });

    // Pedir TODAS as permissões necessárias antes de qualquer scan.
    // Android 6-11: ACCESS_FINE_LOCATION é obrigatório para scan BLE.
    // Android 12+:  BLUETOOTH_SCAN + BLUETOOTH_CONNECT.
    // Sem localização o scan retorna zero resultados silenciosamente.
    final perms = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();

    final scanOk    = perms[Permission.bluetoothScan]?.isGranted    ?? false;
    final connectOk = perms[Permission.bluetoothConnect]?.isGranted ?? false;
    final locOk     = perms[Permission.locationWhenInUse]?.isGranted ?? false;

    // Log das permissões na tela para diagnóstico
    setState(() {
      _scanLog.add('BT scan: ${scanOk ? "OK" : "NEGADO"} '
          'BT connect: ${connectOk ? "OK" : "NEGADO"} '
          'Loc: ${locOk ? "OK" : "NEGADO"}');
    });

    if (!scanOk && !locOk) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
                'Permissão negada. O app precisa de Bluetooth e '
                'Localização para encontrar o aparelho. '
                'Conceda em Configuracoes > Apps > elderly_app > Permissoes.'),
            backgroundColor: Colors.red,
            duration: Duration(seconds: 7),
          ),
        );
      }
      return;
    }

    // Garantir Bluetooth ligado
    if (await FlutterBluePlus.adapterState.first != BluetoothAdapterState.on) {
      try {
        await FlutterBluePlus.turnOn();
        await FlutterBluePlus.adapterState
            .where((s) => s == BluetoothAdapterState.on)
            .first
            .timeout(const Duration(seconds: 10));
      } catch (_) {
        if (mounted) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(
              content: Text('Ative o Bluetooth e tente novamente.'),
              backgroundColor: Colors.red,
            ),
          );
        }
        return;
      }
    }

    try {
      await _ble.scan(timeoutSec: 25);
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Erro no scan: ${e.toString()}'),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  Future<void> _detectHotspot() async {
    // Tenta detectar se o hotspot do celular está ativo lendo as
    // interfaces de rede. Quando o hotspot está ligado, o Android
    // cria uma interface "wlan_ap" ou "ap0" com um IP na faixa 192.168.43.x
    // ou 192.168.0.x. Não há API pública para ler o SSID do hotspot
    // sem permissão NETWORK_SETTINGS (reservada para apps de sistema),
    // mas conseguimos detectar que está ativo e sugerir ao usuário.
    try {
      final interfaces = await NetworkInterface.list(
          type: InternetAddressType.IPv4);
      for (final iface in interfaces) {
        final name = iface.name.toLowerCase();
        // Interfaces típicas de hotspot no Android
        if (name.contains('ap') || name.contains('wlan1') ||
            name.contains('swlan')) {
          if (mounted) {
            setState(() => _hotspotSsid = '(hotspot ativo)');
          }
          return;
        }
      }
    } catch (_) {}
  }

  Future<void> _scanWifi() async {
    setState(() => _loadingWifi = true);
    try {
      // wifi_scan: verificar se pode escanear
      final can = await WiFiScan.instance.canStartScan(askPermissions: true);
      if (can == CanStartScan.yes) {
        await WiFiScan.instance.startScan();
        final results = await WiFiScan.instance.getScannedResults();
        // Remover duplicatas por SSID, ordenar por sinal
        final seen = <String>{};
        final unique = results
            .where((r) => r.ssid.isNotEmpty && seen.add(r.ssid))
            .toList()
          ..sort((a, b) => b.level.compareTo(a.level));
        if (mounted) setState(() => _wifiNetworks = unique);
      }
    } catch (_) {}
    if (mounted) setState(() => _loadingWifi = false);
  }

  Future<void> _connectTo(BluetoothDevice device) async {
    try {
      await _ble.connect(device);
      if (!mounted) return;
      setState(() => _selectedDevice = device);
      // Atualizar lista WiFi se ainda vazia (usuário chegou direto no passo 2)
      if (_wifiNetworks.isEmpty) _scanWifi();
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Falha ao conectar: $e'),
            backgroundColor: Colors.red),
        );
      }
    }
  }

  Future<void> _provision() async {
    if (!_formKey.currentState!.validate()) return;

    final result = await _ble.provision(
      ssid:     _ssidController.text.trim(),
      password: _passController.text,
      deviceId: widget.deviceId.toString(),
    );

    if (!mounted) return;

    if (result.success) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Dispositivo "${result.deviceId}" configurado!'),
          backgroundColor: Colors.green,
        ),
      );
      // Aguardar o dispositivo reiniciar antes de fechar
      await Future.delayed(const Duration(seconds: 1));
      if (mounted) Navigator.of(context).pop(result.deviceId);
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Erro: ${result.error}'),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    final bool busy = _state == BleState.scanning    ||
                      _state == BleState.connecting  ||
                      _state == BleState.provisioning;

    return Scaffold(
      appBar: AppBar(
        title: const Text('Configurar Aparelho',
            style: TextStyle(color: Colors.white)),
        backgroundColor: const Color(0xFF1a3a52),
        iconTheme: const IconThemeData(color: Colors.white),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [

            // ── Status bar ──────────────────────────────────────────────────
            _StatusCard(state: _state, message: _statusMsg, busy: busy),
            const SizedBox(height: 20),

            // ── Passo 1: Scan / lista de dispositivos ───────────────────────
            if (_state != BleState.connected  &&
                _state != BleState.provisioning &&
                _state != BleState.done) ...[
              Row(
                children: [
                  const Text('1. Encontrar aparelho',
                      style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
                  const Spacer(),
                  if (!busy)
                    TextButton.icon(
                      onPressed: _startScan,
                      icon: const Icon(Icons.refresh),
                      label: const Text('Buscar novamente'),
                    ),
                ],
              ),
              const SizedBox(height: 8),

              if (_state == BleState.scanning)
                const Center(child: CircularProgressIndicator())
              else if (_found.isEmpty)
                const _EmptyHint()
              else
                ..._found.map((d) => _DeviceTile(
                  device: d,
                  onTap: busy ? null : () => _connectTo(d),
                )),
              // ── Todos os dispositivos (debug) ──────────────────────────
              if (_allResults.isNotEmpty)
                _buildAllDevicesDebug(),
              const SizedBox(height: 24),
            ],

            // ── Passo 2: Formulário de configuração ────────────────────────
            if (_state == BleState.connected  ||
                _state == BleState.provisioning ||
                _state == BleState.done) ...[
              Text(
                'Aparelho: ${_selectedDevice?.platformName ?? "ElderGuard"}',
                style: const TextStyle(fontWeight: FontWeight.w500,
                    color: Color(0xFF1a3a52)),
              ),
              const SizedBox(height: 16),
              const Text('2. Configurar WiFi e ID',
                  style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16)),
              const SizedBox(height: 12),

              Form(
                key: _formKey,
                child: Column(children: [
                  // Aviso hotspot
                  if (_hotspotSsid != null)
                    Card(
                      color: Colors.blue.withOpacity(0.08),
                      shape: RoundedRectangleBorder(
                          side: BorderSide(color: Colors.blue.withOpacity(0.4)),
                          borderRadius: BorderRadius.circular(8)),
                      child: Padding(
                        padding: const EdgeInsets.all(12),
                        child: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            const Row(children: [
                              Icon(Icons.wifi_tethering,
                                  color: Colors.blue, size: 18),
                              SizedBox(width: 8),
                              Text('Hotspot detectado',
                                  style: TextStyle(
                                      fontWeight: FontWeight.bold,
                                      color: Colors.blue)),
                            ]),
                            const SizedBox(height: 6),
                            const Text(
                              'Hotspot ativo. O SSID nao aparece na lista '
                              '(limitacao do Android). Digite manualmente.',
                              style: TextStyle(fontSize: 12),
                            ),
                          ],
                        ),
                      ),
                    ),
                  const SizedBox(height: 8),
                  // Redes WiFi
                  if (_loadingWifi)
                    const Padding(
                      padding: EdgeInsets.symmetric(vertical: 8),
                      child: Row(children: [
                        SizedBox(width: 16, height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2)),
                        SizedBox(width: 8),
                        Text('Buscando redes WiFi...',
                            style: TextStyle(fontSize: 12)),
                      ]),
                    )
                  else if (_wifiNetworks.isNotEmpty) ...[
                    DropdownButtonFormField<String>(
                      decoration: _inputDeco('Selecionar rede WiFi', Icons.wifi),
                      value: _ssidController.text.isEmpty
                          ? null : _ssidController.text,
                      hint: const Text('Escolha uma rede'),
                      isExpanded: true,
                      items: _wifiNetworks.map((ap) {
                        final bars = ap.level > -50 ? 4
                            : ap.level > -65 ? 3
                            : ap.level > -75 ? 2 : 1;
                        return DropdownMenuItem(
                          value: ap.ssid,
                          child: Row(children: [
                            Icon(Icons.signal_wifi_4_bar,
                                size: 16,
                                color: bars >= 3 ? Colors.green
                                    : bars == 2 ? Colors.orange
                                    : Colors.red),
                            const SizedBox(width: 6),
                            Expanded(child: Text(ap.ssid,
                                overflow: TextOverflow.ellipsis)),
                            Text('${ap.level} dBm',
                                style: const TextStyle(
                                    fontSize: 10, color: Colors.grey)),
                          ]),
                        );
                      }).toList(),
                      onChanged: busy || _state == BleState.done
                          ? null
                          : (v) {
                              if (v != null) {
                                setState(() => _ssidController.text = v);
                              }
                            },
                      validator: (_) => _ssidController.text.trim().isEmpty
                          ? 'Selecione ou digite o SSID' : null,
                    ),
                    const SizedBox(height: 6),
                    TextButton.icon(
                      onPressed: busy ? null : _scanWifi,
                      icon: const Icon(Icons.refresh, size: 16),
                      label: const Text('Atualizar redes',
                          style: TextStyle(fontSize: 12)),
                    ),
                  ],
                  // SSID manual
                  TextFormField(
                    controller: _ssidController,
                    enabled: !busy && _state != BleState.done,
                    decoration: _inputDeco(
                        _wifiNetworks.isEmpty
                            ? 'Nome da rede WiFi (SSID)'
                            : 'Ou digitar SSID manualmente',
                        Icons.edit),
                    validator: (v) => (v == null || v.trim().isEmpty)
                        ? 'SSID obrigatorio' : null,
                  ),
                  const SizedBox(height: 12),
                  // Senha
                  TextFormField(
                    controller: _passController,
                    enabled: !busy && _state != BleState.done,
                    obscureText: _obscurePass,
                    decoration: _inputDeco(
                      'Senha WiFi (vazio para rede aberta)',
                      Icons.lock,
                    ).copyWith(
                      suffixIcon: IconButton(
                        icon: Icon(_obscurePass
                            ? Icons.visibility_off
                            : Icons.visibility),
                        onPressed: () =>
                            setState(() => _obscurePass = !_obscurePass),
                      ),
                    ),
                  ),
                  const SizedBox(height: 12),
                  const Text(
                    'O ID sera atribuido automaticamente pelo servidor.',
                    style: TextStyle(fontSize: 12, color: Colors.grey),
                  ),
                ]),  // Column
              ),    // Form
              const SizedBox(height: 20),

              if (_state != BleState.done)
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton.icon(
                    style: ElevatedButton.styleFrom(
                      backgroundColor: const Color(0xFF1a3a52),
                      padding: const EdgeInsets.symmetric(vertical: 14),
                    ),
                    onPressed: busy ? null : _provision,
                    icon: busy
                        ? const SizedBox(
                            width: 18,
                            height: 18,
                            child: CircularProgressIndicator(
                              strokeWidth: 2,
                              valueColor:
                                  AlwaysStoppedAnimation(Colors.white),
                            ),
                          )
                        : const Icon(Icons.send, color: Colors.white),
                    label: Text(
                      busy ? 'Aguarde…' : 'Enviar configurações',
                      style: const TextStyle(color: Colors.white, fontSize: 16),
                    ),
                  ),
                ),

              const SizedBox(height: 12),
              TextButton(
                onPressed: busy ? null : () async {
                  await _ble.disconnect();
                  if (mounted) {
                    setState(() {
                      _selectedDevice = null;
                      _found = [];
                    });
                    _startScan();
                  }
                },
                child: const Text('← Escolher outro aparelho'),
              ),
              const Divider(height: 24),
              // ── Reset de fábrica ──────────────────────────────────────
              OutlinedButton.icon(
                style: OutlinedButton.styleFrom(
                  foregroundColor: Colors.red,
                  side: const BorderSide(color: Colors.red),
                ),
                onPressed: busy ? null : _confirmReset,
                icon: const Icon(Icons.restore),
                label: const Text('Resetar provisionamento'),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _buildAllDevicesDebug() {
    return ExpansionTile(
      tilePadding: EdgeInsets.zero,
      title: Text(
        'Dispositivos detectados (${_allResults.length})',
        style: const TextStyle(fontSize: 12, color: Colors.grey),
      ),
      children: [
        // Log de texto
        if (_scanLog.isNotEmpty)
          Container(
            margin: const EdgeInsets.symmetric(horizontal: 4, vertical: 4),
            padding: const EdgeInsets.all(8),
            decoration: BoxDecoration(
              color: Colors.black87,
              borderRadius: BorderRadius.circular(6),
            ),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: _scanLog.map((l) => Text(l,
                style: const TextStyle(
                  fontSize: 10, color: Colors.greenAccent,
                  fontFamily: 'monospace'),
              )).toList(),
            ),
          ),
        // Lista de todos os devices
        ..._allResults.map((r) {
          final name = r.device.platformName.isNotEmpty
              ? r.device.platformName
              : r.advertisementData.advName.isNotEmpty
                  ? r.advertisementData.advName
                  : '(sem nome)';
          final uuids = r.advertisementData.serviceUuids
              .map((u) => u.toString().toLowerCase())
              .join(', ');
          // Verificar se é ElderGuard por qualquer critério
          final isEG = name == 'ElderGuard' ||
              r.advertisementData.advName == 'ElderGuard' ||
              uuids.contains('4fafc201');
          return ListTile(
            dense: true,
            tileColor: isEG ? Colors.green.withOpacity(0.08) : null,
            leading: Icon(Icons.bluetooth,
                size: 16, color: isEG ? Colors.green : Colors.grey),
            title: Text('$name ${isEG ? "✓" : ""}',
                style: TextStyle(
                    fontSize: 12,
                    color: isEG ? Colors.green : null,
                    fontWeight: isEG ? FontWeight.bold : null)),
            subtitle: Text(
              'ID: ${r.device.remoteId}\nuuids: ${uuids.isEmpty ? "(nenhum)" : uuids}',
              style: const TextStyle(fontSize: 10, color: Colors.grey),
            ),
            trailing: isEG
                ? ElevatedButton(
                    style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF1a3a52)),
                    onPressed: () => _connectTo(r.device),
                    child: const Text('Conectar',
                        style: TextStyle(color: Colors.white, fontSize: 12)),
                  )
                : null,
          );
        }),
      ],
    );
  }

  Future<void> _confirmReset() async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Resetar dispositivo'),
        content: const Text(
            'Isso apagará o WiFi, device ID e todas as configurações '
            'salvas no aparelho. Ele voltará ao modo de provisionamento.\n\n'
            'Deseja continuar?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancelar'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Resetar', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
    if (confirm != true || !mounted) return;

    final ok = await _ble.sendCommand('reset');
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(ok
              ? 'Reset enviado. O aparelho reiniciará em modo BLE.'
              : 'Falha ao enviar reset. Verifique a conexão.'),
          backgroundColor: ok ? Colors.green : Colors.red,
        ),
      );
      if (ok) {
        await _ble.disconnect();
        setState(() { _selectedDevice = null; _found = []; });
        await Future.delayed(const Duration(seconds: 2));
        if (mounted) _startScan();
      }
    }
  }

  InputDecoration _inputDeco(String label, IconData icon) => InputDecoration(
        labelText: label,
        prefixIcon: Icon(icon),
        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
      );
}

// ── Widgets auxiliares ────────────────────────────────────────────────────────

class _StatusCard extends StatelessWidget {
  final BleState state;
  final String message;
  final bool busy;

  const _StatusCard({required this.state, required this.message, required this.busy});

  Color get _color {
    switch (state) {
      case BleState.done:  return Colors.green;
      case BleState.error: return Colors.red;
      default:             return const Color(0xFF1a3a52);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      color: _color.withOpacity(0.08),
      shape: RoundedRectangleBorder(
          side: BorderSide(color: _color.withOpacity(0.4)),
          borderRadius: BorderRadius.circular(8)),
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Row(
          children: [
            if (busy)
              const SizedBox(
                  width: 18,
                  height: 18,
                  child: CircularProgressIndicator(strokeWidth: 2))
            else
              Icon(_stateIcon(state), color: _color, size: 20),
            const SizedBox(width: 12),
            Expanded(
              child: Text(message,
                  style: TextStyle(color: _color, fontWeight: FontWeight.w500)),
            ),
          ],
        ),
      ),
    );
  }

  IconData _stateIcon(BleState s) {
    switch (s) {
      case BleState.connected:    return Icons.bluetooth_connected;
      case BleState.done:         return Icons.check_circle;
      case BleState.error:        return Icons.error;
      default:                    return Icons.bluetooth_searching;
    }
  }
}

class _DeviceTile extends StatelessWidget {
  final BluetoothDevice device;
  final VoidCallback? onTap;

  const _DeviceTile({required this.device, this.onTap});

  @override
  Widget build(BuildContext context) {
    return Card(
      margin: const EdgeInsets.symmetric(vertical: 4),
      child: ListTile(
        leading: const Icon(Icons.watch, color: Color(0xFF1a3a52)),
        title: Text(device.platformName.isNotEmpty
            ? device.platformName
            : 'ElderGuard'),
        subtitle: Text(device.remoteId.toString(),
            style: const TextStyle(fontSize: 12)),
        trailing: ElevatedButton(
          style: ElevatedButton.styleFrom(
              backgroundColor: const Color(0xFF1a3a52)),
          onPressed: onTap,
          child: const Text('Conectar',
              style: TextStyle(color: Colors.white)),
        ),
      ),
    );
  }
}

class _EmptyHint extends StatelessWidget {
  const _EmptyHint();

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.all(20),
      alignment: Alignment.center,
      child: const Column(
        children: [
          Icon(Icons.bluetooth_disabled, size: 48, color: Colors.grey),
          SizedBox(height: 12),
          Text('Nenhum aparelho ElderGuard encontrado.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.grey)),
          SizedBox(height: 6),
          Text(
            'Verifique se o aparelho está ligado e próximo,\n'
            'e se o Bluetooth do celular está ativo.',
            textAlign: TextAlign.center,
            style: TextStyle(fontSize: 12, color: Colors.grey),
          ),
        ],
      ),
    );
  }
}
