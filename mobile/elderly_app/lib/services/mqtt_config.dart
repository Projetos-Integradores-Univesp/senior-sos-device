import 'package:shared_preferences/shared_preferences.dart';

// ============================================================================
// MqttConfig — configuração do broker MQTT
//
// Os valores padrão são lidos de variáveis de ambiente injetadas no build
// via --dart-define (CI/CD) ou de um arquivo .env local (nunca versionado).
//
// Para build local:
//   flutter run \
//     --dart-define=MQTT_HOST=seu-broker.exemplo.com \
//     --dart-define=MQTT_USERNAME=seu-usuario \
//     --dart-define=MQTT_PASSWORD=sua-senha
//
// No GitHub Actions, usar Repository Secrets e injetar via:
//   flutter build apk \
//     --dart-define=MQTT_HOST=\${{ secrets.SECRET_MQTT_BROKER }} \
//     --dart-define=MQTT_USERNAME=\${{ secrets.SECRET_MQTT_USERNAME }} \
//     --dart-define=MQTT_PASSWORD=\${{ secrets.SECRET_MQTT_PASSWORD }}
//
// Persistido em SharedPreferences; valores lidos no início de cada sessão.
// ============================================================================
class MqttConfig {
  final String host;
  final int    port;
  final String username;
  final String password;
  final bool   useTls;

  const MqttConfig({
    required this.host,
    required this.port,
    required this.username,
    required this.password,
    required this.useTls,
  });

  // Padrões lidos de --dart-define em tempo de build.
  // Nunca commitar valores reais aqui.
  static const String _defaultHost     = String.fromEnvironment('MQTT_HOST',     defaultValue: '');
  static const String _defaultUsername = String.fromEnvironment('MQTT_USERNAME', defaultValue: '');
  static const String _defaultPassword = String.fromEnvironment('MQTT_PASSWORD', defaultValue: '');
  static const int    _defaultPort     = int.fromEnvironment('MQTT_PORT',        defaultValue: 1883);
  static const bool   _defaultTls      = bool.fromEnvironment('MQTT_TLS',        defaultValue: false);

  static const MqttConfig defaults = MqttConfig(
    host:     _defaultHost,
    port:     _defaultPort,
    username: _defaultUsername,
    password: _defaultPassword,
    useTls:   _defaultTls,
  );

  MqttConfig copyWith({
    String? host,
    int?    port,
    String? username,
    String? password,
    bool?   useTls,
  }) => MqttConfig(
    host:     host     ?? this.host,
    port:     port     ?? this.port,
    username: username ?? this.username,
    password: password ?? this.password,
    useTls:   useTls   ?? this.useTls,
  );

  // ── Persistência ─────────────────────────────────────────────────────────

  static const _kHost     = 'mqtt_host';
  static const _kPort     = 'mqtt_port';
  static const _kUser     = 'mqtt_user';
  static const _kPass     = 'mqtt_pass';
  static const _kTls      = 'mqtt_tls';

  static Future<MqttConfig> load() async {
    final p = await SharedPreferences.getInstance();
    return MqttConfig(
      host:     p.getString(_kHost)  ?? defaults.host,
      port:     p.getInt(_kPort)     ?? defaults.port,
      username: p.getString(_kUser)  ?? defaults.username,
      password: p.getString(_kPass)  ?? defaults.password,
      useTls:   p.getBool(_kTls)     ?? defaults.useTls,
    );
  }

  Future<void> save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString(_kHost,  host);
    await p.setInt(_kPort,     port);
    await p.setString(_kUser,  username);
    await p.setString(_kPass,  password);
    await p.setBool(_kTls,     useTls);
  }

  static Future<void> reset() async {
    final p = await SharedPreferences.getInstance();
    for (final k in [_kHost, _kPort, _kUser, _kPass, _kTls]) {
      await p.remove(k);
    }
  }

  @override
  String toString() =>
      'MqttConfig(${useTls ? "mqtts" : "mqtt"}://$username@$host:$port)';
}
