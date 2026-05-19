import 'package:flutter/material.dart';
import '../services/mqtt_config.dart';
import '../services/mqtt_service.dart';

// ============================================================================
// MqttSettingsScreen — configuração do broker MQTT
//
// Campos editáveis:
//   • Endereço do broker (host)
//   • Porta  (1883 padrão / 8883 com TLS)
//   • Usuário
//   • Senha
//   • Usar TLS (switch) — ajusta a porta automaticamente se ainda for o padrão
//
// Ao salvar: persiste via MqttConfig.save() e reconecta o MqttService.
// Botão "Restaurar padrões" reverte para os valores de --dart-define do build.
// ============================================================================
class MqttSettingsScreen extends StatefulWidget {
  final MqttService   mqttService;
  final MqttConfig    currentConfig;
  final List<String>  deviceIds;

  const MqttSettingsScreen({
    super.key,
    required this.mqttService,
    required this.currentConfig,
    required this.deviceIds,
  });

  @override
  State<MqttSettingsScreen> createState() => _MqttSettingsScreenState();
}

class _MqttSettingsScreenState extends State<MqttSettingsScreen> {
  late TextEditingController _hostCtrl;
  late TextEditingController _portCtrl;
  late TextEditingController _userCtrl;
  late TextEditingController _passCtrl;
  late bool _useTls;
  late bool _obscurePass;
  bool _isSaving = false;

  final _formKey = GlobalKey<FormState>();

  // Portas padrão
  static const int _portPlain = 1883;
  static const int _portTls   = 8883;

  @override
  void initState() {
    super.initState();
    final c = widget.currentConfig;
    _hostCtrl    = TextEditingController(text: c.host);
    _portCtrl    = TextEditingController(text: c.port.toString());
    _userCtrl    = TextEditingController(text: c.username);
    _passCtrl    = TextEditingController(text: c.password);
    _useTls      = c.useTls;
    _obscurePass = true;
  }

  @override
  void dispose() {
    _hostCtrl.dispose();
    _portCtrl.dispose();
    _userCtrl.dispose();
    _passCtrl.dispose();
    super.dispose();
  }

  void _onTlsChanged(bool value) {
    setState(() {
      _useTls = value;
      // Ajustar porta automaticamente apenas se ainda for a padrão oposta
      final currentPort = int.tryParse(_portCtrl.text) ?? _portPlain;
      if (!value && currentPort == _portTls) {
        _portCtrl.text = _portPlain.toString();
      } else if (value && currentPort == _portPlain) {
        _portCtrl.text = _portTls.toString();
      }
    });
  }

  Future<void> _save() async {
    if (!_formKey.currentState!.validate()) return;

    setState(() => _isSaving = true);

    final newConfig = MqttConfig(
      host:     _hostCtrl.text.trim(),
      port:     int.parse(_portCtrl.text.trim()),
      username: _userCtrl.text.trim(),
      password: _passCtrl.text,
      useTls:   _useTls,
    );

    try {
      await newConfig.save();
      // Reconectar com a nova configuração
      await widget.mqttService.connect(
        newConfig,
        deviceIds: widget.deviceIds,
      );
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Configurações salvas e broker reconectado!'),
            backgroundColor: Colors.green,
          ),
        );
        Navigator.of(context).pop(newConfig);
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text('Falha ao conectar: $e'),
            backgroundColor: Colors.red,
          ),
        );
      }
    } finally {
      if (mounted) setState(() => _isSaving = false);
    }
  }

  Future<void> _restoreDefaults() async {
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Restaurar padrões'),
        content: const Text(
            'Restaurar os valores padrão de build (definidos via --dart-define)?'),
        actions: [
          TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancelar')),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1a3a52)),
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Restaurar',
                style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
    if (confirm != true || !mounted) return;

    await MqttConfig.reset();
    final d = MqttConfig.defaults;
    setState(() {
      _hostCtrl.text = d.host;
      _portCtrl.text = d.port.toString();
      _userCtrl.text = d.username;
      _passCtrl.text = d.password;
      _useTls        = d.useTls;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Configuração MQTT',
            style: TextStyle(color: Colors.white)),
        backgroundColor: const Color(0xFF1a3a52),
        iconTheme: const IconThemeData(color: Colors.white),
        actions: [
          IconButton(
            icon: const Icon(Icons.restore),
            tooltip: 'Restaurar padrões',
            onPressed: _isSaving ? null : _restoreDefaults,
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Form(
          key: _formKey,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [

              // ── Status atual ───────────────────────────────────────────────
              _MqttStatusCard(mqttService: widget.mqttService),
              const SizedBox(height: 24),

              const _SectionTitle('Broker'),

              // Host
              TextFormField(
                controller: _hostCtrl,
                enabled: !_isSaving,
                keyboardType: TextInputType.url,
                decoration: _deco('Endereço do broker', Icons.dns),
                validator: (v) =>
                    (v == null || v.trim().isEmpty) ? 'Campo obrigatório' : null,
              ),
              const SizedBox(height: 12),

              // Porta
              TextFormField(
                controller: _portCtrl,
                enabled: !_isSaving,
                keyboardType: TextInputType.number,
                decoration: _deco('Porta', Icons.electrical_services),
                validator: (v) {
                  final n = int.tryParse(v ?? '');
                  if (n == null || n < 1 || n > 65535) {
                    return 'Porta inválida (1–65535)';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 12),

              // TLS switch
              Card(
                shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(8)),
                child: SwitchListTile(
                  secondary: Icon(
                    _useTls ? Icons.lock : Icons.lock_open,
                    color: _useTls ? Colors.green : Colors.grey,
                  ),
                  title: const Text('Usar TLS / SSL'),
                  subtitle: Text(_useTls
                      ? 'Conexão criptografada (porta padrão 8883)'
                      : 'Conexão sem criptografia (porta padrão 1883)'),
                  value: _useTls,
                  onChanged: _isSaving ? null : _onTlsChanged,
                  activeColor: Colors.green,
                ),
              ),
              const SizedBox(height: 20),

              const _SectionTitle('Autenticação'),

              // Usuário
              TextFormField(
                controller: _userCtrl,
                enabled: !_isSaving,
                decoration: _deco('Usuário', Icons.person),
              ),
              const SizedBox(height: 12),

              // Senha
              TextFormField(
                controller: _passCtrl,
                enabled: !_isSaving,
                obscureText: _obscurePass,
                decoration: _deco('Senha', Icons.key).copyWith(
                  suffixIcon: IconButton(
                    icon: Icon(_obscurePass
                        ? Icons.visibility_off
                        : Icons.visibility),
                    onPressed: () =>
                        setState(() => _obscurePass = !_obscurePass),
                  ),
                ),
              ),
              const SizedBox(height: 28),

              // Botão salvar
              SizedBox(
                width: double.infinity,
                child: ElevatedButton.icon(
                  style: ElevatedButton.styleFrom(
                    backgroundColor: const Color(0xFF1a3a52),
                    padding: const EdgeInsets.symmetric(vertical: 14),
                  ),
                  onPressed: _isSaving ? null : _save,
                  icon: _isSaving
                      ? const SizedBox(
                          width: 18,
                          height: 18,
                          child: CircularProgressIndicator(
                            strokeWidth: 2,
                            valueColor:
                                AlwaysStoppedAnimation(Colors.white),
                          ),
                        )
                      : const Icon(Icons.save, color: Colors.white),
                  label: Text(
                    _isSaving ? 'Conectando…' : 'Salvar e reconectar',
                    style: const TextStyle(color: Colors.white, fontSize: 16),
                  ),
                ),
              ),
              const SizedBox(height: 12),

              // Info sobre tópicos
              const _TopicInfoCard(),
            ],
          ),
        ),
      ),
    );
  }

  InputDecoration _deco(String label, IconData icon) => InputDecoration(
        labelText: label,
        prefixIcon: Icon(icon),
        border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
      );
}

// ── Widgets auxiliares ────────────────────────────────────────────────────────

class _SectionTitle extends StatelessWidget {
  final String text;
  const _SectionTitle(this.text);
  @override
  Widget build(BuildContext context) => Padding(
        padding: const EdgeInsets.only(bottom: 10),
        child: Text(text,
            style: const TextStyle(
                fontWeight: FontWeight.bold,
                fontSize: 15,
                color: Color(0xFF1a3a52))),
      );
}

class _MqttStatusCard extends StatelessWidget {
  final MqttService mqttService;
  const _MqttStatusCard({required this.mqttService});

  @override
  Widget build(BuildContext context) {
    return StreamBuilder<MqttConnState>(
      stream: mqttService.connStateStream,
      initialData: mqttService.connState,
      builder: (_, snap) {
        final state = snap.data ?? MqttConnState.disconnected;
        final (icon, color, label) = switch (state) {
          MqttConnState.connected    => (Icons.check_circle, Colors.green,    'Conectado'),
          MqttConnState.connecting   => (Icons.sync,         Colors.orange,   'Conectando…'),
          MqttConnState.error        => (Icons.error,        Colors.red,      'Erro de conexão'),
          MqttConnState.disconnected => (Icons.cloud_off,    Colors.grey,     'Desconectado'),
        };
        return Card(
          color: color.withOpacity(0.08),
          shape: RoundedRectangleBorder(
              side: BorderSide(color: color.withOpacity(0.4)),
              borderRadius: BorderRadius.circular(8)),
          child: ListTile(
            leading: Icon(icon, color: color),
            title: Text('Status: $label',
                style: TextStyle(color: color, fontWeight: FontWeight.w600)),
            subtitle: Text(
              'Dispositivos monitorados: '
              '${mqttService.subscribedDeviceIds.isEmpty ? "nenhum" : mqttService.subscribedDeviceIds.join(", ")}',
              style: const TextStyle(fontSize: 12),
            ),
          ),
        );
      },
    );
  }
}

class _TopicInfoCard extends StatelessWidget {
  const _TopicInfoCard();
  @override
  Widget build(BuildContext context) {
    return Card(
      color: Colors.blue.withOpacity(0.05),
      shape: RoundedRectangleBorder(
          side: BorderSide(color: Colors.blue.withOpacity(0.3)),
          borderRadius: BorderRadius.circular(8)),
      child: const Padding(
        padding: EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(children: [
              Icon(Icons.info_outline, color: Colors.blue, size: 18),
              SizedBox(width: 8),
              Text('Tópicos monitorados',
                  style: TextStyle(
                      fontWeight: FontWeight.bold, color: Colors.blue)),
            ]),
            SizedBox(height: 8),
            Text('devices/{id}/fall',
                style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
            Text('devices/{id}/button-pressed',
                style: TextStyle(fontFamily: 'monospace', fontSize: 12)),
            SizedBox(height: 6),
            Text(
              'O ID do dispositivo é definido durante o provisionamento BLE.',
              style: TextStyle(fontSize: 12, color: Colors.grey),
            ),
          ],
        ),
      ),
    );
  }
}
