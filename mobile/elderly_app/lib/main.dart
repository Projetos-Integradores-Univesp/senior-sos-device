import 'dart:async';
import 'package:flutter/material.dart';
import 'services/api_service.dart';
import 'services/mqtt_config.dart';
import 'services/notification_service.dart';
import 'services/foreground_service.dart';
import 'services/mqtt_service.dart';
import 'screens/provision_screen.dart';
import 'screens/mqtt_settings_screen.dart';

// Enum para tipo de alerta
enum AlertSeverity { info, warning, error }

// Modelo para alerta
class Alert {
  final String id;
  final String title;
  final String message;
  final DateTime timestamp;
  final String deviceName;
  final AlertSeverity severity;

  Alert({
    required this.id,
    required this.title,
    required this.message,
    required this.timestamp,
    required this.deviceName,
    required this.severity,
  });
}

// Modelo para dispositivo
class Device {
  final String id;
  final String name;
  final int    deviceId;   // ID inteiro atribuído pelo backend
  final DateTime dateAdded;

  Device({
    required this.id,
    required this.name,
    required this.deviceId,
    required this.dateAdded,
  });

  // Criar uma cópia do dispositivo com campos modificados
  Device copyWith({
    String? id,
    String? name,
    int?    deviceId,
    DateTime? dateAdded,
  }) {
    return Device(
      id: id ?? this.id,
      name: name ?? this.name,
      deviceId: deviceId ?? this.deviceId,
      dateAdded: dateAdded ?? this.dateAdded,
    );
  }
}

// Instâncias globais de MQTT (criadas uma única vez no main)
MqttService? _globalMqttService;
MqttConfig?  _globalMqttConfig;

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  _globalMqttConfig  = await MqttConfig.load();
  _globalMqttService = MqttService();
  // Inicializar notificações do sistema
  await NotificationService.init();
  // Configurar foreground service (sem iniciar ainda)
  initForegroundTask();
  // Conectar em background; falhas são exibidas na aba MQTT
  _globalMqttService!.connect(_globalMqttConfig!).catchError((_) {});
  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SOS IDOSO',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        primaryColor: const Color(0xFF1a3a52), // Azul escuro
        useMaterial3: true,
        appBarTheme: const AppBarTheme(
          backgroundColor: Color(0xFF1a3a52),
          elevation: 2,
        ),
      ),
      home: const IntroScreen(),
    );
  }
}

class IntroScreen extends StatelessWidget {
  const IntroScreen({super.key});

  static const String loreText = 'Você recebeu seu Aparelho? Vamos ativar?';

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('SOS IDOSO', style: TextStyle(color: Colors.white)),
      ),
      body: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              'Como usar o aparelho',
              style: TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.bold,
                color: Color(0xFF1a3a52),
              ),
            ),
            const SizedBox(height: 16),
            Text(IntroScreen.loreText, style: const TextStyle(fontSize: 16)),
            const SizedBox(height: 24),
            const Text(
              '- Faça o login ou cadastre-se para acessar as funcionalidades.',
            ),
            const Text(
              '- Após o acessso, com o aparelho conectado aperte o botão para...',
            ),
            const SizedBox(height: 24),
            Center(
              child: ElevatedButton(
                style: ElevatedButton.styleFrom(
                  backgroundColor: const Color(0xFF1a3a52),
                ),
                onPressed: () {
                  Navigator.of(context).pushReplacement(
                    MaterialPageRoute(builder: (_) => const HomePage()),
                  );
                },
                child: const Text(
                  'Começar',
                  style: TextStyle(color: Colors.white),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage>
    with SingleTickerProviderStateMixin {
  late TabController _tabController;

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          'SOS IDOSO',
          style: TextStyle(
            fontSize: 24,
            fontWeight: FontWeight.bold,
            color: Colors.white,
          ),
        ),
        bottom: TabBar(
          controller: _tabController,
          tabs: const [
            Tab(
              text: 'Login',
              icon: Icon(Icons.login),
              iconMargin: EdgeInsets.only(bottom: 6),
            ),
            Tab(
              text: 'Contato',
              icon: Icon(Icons.phone),
              iconMargin: EdgeInsets.only(bottom: 6),
            ),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          // Aba Login
          Container(color: Colors.white, child: const LoginPage()),
          // Aba Contato
          Container(color: Colors.white, child: const ContatoPage()),
        ],
      ),
    );
  }
}

class LoginPage extends StatefulWidget {
  const LoginPage({super.key});

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final _usernameController = TextEditingController();
  final _senhaController = TextEditingController();
  bool _isLoading = false;

  @override
  void dispose() {
    _usernameController.dispose();
    _senhaController.dispose();
    super.dispose();
  }

  Future<void> _login() async {
    if (_usernameController.text.isEmpty || _senhaController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Por favor, preencha todos os campos')),
      );
      return;
    }

    setState(() => _isLoading = true);

    final result = await ApiService.login(
      _usernameController.text,
      _senhaController.text,
    );

    setState(() => _isLoading = false);

    if (!mounted) return;

    if (result['success']) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(result['message'])),
      );
      // Navegar para a página de gerenciamento de dispositivos
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(
          builder: (_) {
          // MqttService e MqttConfig injetados na navegação
          // São criados e conectados em MqttStartup (ver main())
          final svc = _globalMqttService!;
          final cfg = _globalMqttConfig!;
          return DeviceManagementPage(
            username:    _usernameController.text,
            mqttService: svc,
            mqttConfig:  cfg,
          );
        },
        ),
      );
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(result['message']),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _deleteAccount() async {
    // Solicitar confirmação
    final shouldDelete = await showDialog<bool>(
      context: context,
      builder: (BuildContext context) => AlertDialog(
        title: const Text('Confirmar exclusão de conta'),
        content: const Text(
          'Esta ação é irreversível. Sua conta e todos os dados associados serão deletados permanentemente. Tem certeza?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Deletar', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );

    if (shouldDelete != true) return;

    setState(() => _isLoading = true);

    final result = await ApiService.deleteAccount();

    setState(() => _isLoading = false);

    if (!mounted) return;

    if (result['success']) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(result['message'])),
      );
      // Retornar à tela inicial
      Navigator.of(context).pushReplacementNamed('/');
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(result['message']),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _showEditAccountDialog() async {
    showDialog(
      context: context,
      builder: (context) => EditAccountDialog(
        currentUsername: _usernameController.text,
        onSave: (newUsername, newPassword) async {
          final result = await ApiService.updateAccount(newUsername, newPassword);

          if (mounted) {
            if (result['success']) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text(result['message'])),
              );
              // Atualizar o controlador com o novo username
              _usernameController.text = newUsername;
            } else {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(result['message']),
                  backgroundColor: Colors.red,
                ),
              );
            }
          }
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Text(
              'Bem-vindo',
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: Color(0xFF1a3a52),
              ),
            ),
            const SizedBox(height: 32),
            TextField(
              controller: _usernameController,
              enabled: !_isLoading,
              decoration: InputDecoration(
                labelText: 'Nome de Usuário',
                hintText: 'Digite seu nome de usuário',
                prefixIcon: const Icon(Icons.person),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _senhaController,
              enabled: !_isLoading,
              obscureText: true,
              decoration: InputDecoration(
                labelText: 'Senha',
                hintText: 'Digite sua senha',
                prefixIcon: const Icon(Icons.lock),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1a3a52),
                padding: const EdgeInsets.symmetric(
                  horizontal: 48,
                  vertical: 16,
                ),
              ),
              onPressed: _isLoading ? null : _login,
              child: _isLoading
                  ? const SizedBox(
                      height: 20,
                      width: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                      ),
                    )
                  : const Text(
                      'Entrar',
                      style: TextStyle(fontSize: 16, color: Colors.white),
                    ),
            ),
            const SizedBox(height: 12),
            TextButton(
              onPressed: _isLoading
                  ? null
                  : () {
                      Navigator.of(context).push(
                        MaterialPageRoute(
                            builder: (_) => const RegistrationScreen()),
                      );
                    },
              child: const Text('Cadastre-se'),
            ),
            const SizedBox(height: 8),
            TextButton(
              onPressed: _isLoading ? null : _showEditAccountDialog,
              child: const Text(
                'Editar Conta',
                style: TextStyle(color: Color(0xFF1a3a52)),
              ),
            ),
            const SizedBox(height: 4),
            TextButton(
              onPressed: _isLoading ? null : _deleteAccount,
              child: const Text(
                'Excluir Conta',
                style: TextStyle(color: Colors.red),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class RegistrationScreen extends StatefulWidget {
  const RegistrationScreen({super.key});

  @override
  State<RegistrationScreen> createState() => _RegistrationScreenState();
}

class _RegistrationScreenState extends State<RegistrationScreen> {
  final _usernameController = TextEditingController();
  final _passwordController = TextEditingController();
  bool _isLoading = false;

  @override
  void dispose() {
    _usernameController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  Future<void> _register() async {
    if (_usernameController.text.isEmpty || _passwordController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Por favor, preencha todos os campos')),
      );
      return;
    }

    setState(() => _isLoading = true);

    final result = await ApiService.register(
      _usernameController.text,
      _passwordController.text,
    );

    setState(() => _isLoading = false);

    if (!mounted) return;

    if (result['success']) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(result['message'])),
      );
      Navigator.of(context).pop();
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(result['message']),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Cadastro', style: TextStyle(color: Colors.white)),
      ),
      body: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            TextField(
              controller: _usernameController,
              enabled: !_isLoading,
              decoration: InputDecoration(
                labelText: 'Nome de Usuário',
                hintText: 'Digite seu nome de usuário',
                prefixIcon: const Icon(Icons.person),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _passwordController,
              enabled: !_isLoading,
              obscureText: true,
              decoration: InputDecoration(
                labelText: 'Senha',
                hintText: 'Digite sua senha',
                prefixIcon: const Icon(Icons.lock),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1a3a52),
              ),
              onPressed: _isLoading ? null : _register,
              child: _isLoading
                  ? const SizedBox(
                      height: 20,
                      width: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                      ),
                    )
                  : const Text('Cadastrar', style: TextStyle(color: Colors.white)),
            ),
          ],
        ),
      ),
    );
  }
}

// Página de Gerenciamento de Dispositivos
class DeviceManagementPage extends StatefulWidget {
  final String       username;
  final MqttService  mqttService;
  final MqttConfig   mqttConfig;

  const DeviceManagementPage({
    super.key,
    required this.username,
    required this.mqttService,
    required this.mqttConfig,
  });

  @override
  State<DeviceManagementPage> createState() => _DeviceManagementPageState();
}

class _DeviceManagementPageState extends State<DeviceManagementPage>
    with SingleTickerProviderStateMixin {
  List<Device> devices = [];
  List<Alert> alerts = [];
  late TabController _tabController;
  bool _isLoadingDevices = false;
  MqttConfig _mqttConfig = MqttConfig.defaults;

  // Log de mensagens MQTT (últimas 100)
  final List<String> _mqttLog = [];
  late StreamSubscription<MqttAlert>  _alertSub;
  late StreamSubscription<String>     _logSub;

  @override
  void initState() {
    super.initState();
    _mqttConfig   = widget.mqttConfig;
    _tabController = TabController(length: 3, vsync: this);
    NotificationService.requestPermission();
    _tabController.addListener(() => setState(() {}));
    _loadDevices();
    _startForegroundService();

    // Escutar alertas MQTT e adicionar à aba de alertas
    _alertSub = widget.mqttService.alertStream.listen(_onMqttAlert);
    // Escutar log MQTT
    _logSub   = widget.mqttService.logStream.listen((msg) {
      if (!mounted) return;
      setState(() {
        _mqttLog.insert(0, msg);
        if (_mqttLog.length > 100) _mqttLog.removeLast();
      });
    });
  }

  @override
  void dispose() {
    _alertSub.cancel();
    _logSub.cancel();
    _tabController.dispose();
    super.dispose();
  }

  void _onMqttAlert(MqttAlert mqttAlert) {
    if (!mounted) return;

    // Buscar apelido na lista de dispositivos pelo deviceId
    final matchedDevice = devices.where(
      (d) => d.deviceId.toString() == mqttAlert.deviceId,
    ).firstOrNull;
    final displayName = matchedDevice != null
        ? '${matchedDevice.name} (ID: ${matchedDevice.deviceId})'
        : 'ID: ${mqttAlert.deviceId}';

    setState(() {
      alerts.insert(0, Alert(
        id:         mqttAlert.receivedAt.toIso8601String(),
        title:      mqttAlert.title,
        message:    mqttAlert.message,
        timestamp:  mqttAlert.receivedAt,
        deviceName: displayName,
        severity:   mqttAlert.type == 'fall'
                      ? AlertSeverity.error
                      : AlertSeverity.warning,
      ));
    });

    // Notificação do sistema
    if (mqttAlert.type == 'fall') {
      NotificationService.showFallAlert(
        deviceName: matchedDevice?.name ?? mqttAlert.deviceId,
        deviceId:   mqttAlert.deviceId,
        extra:      mqttAlert.message,
      );
    } else {
      NotificationService.showPanicAlert(
        deviceName: matchedDevice?.name ?? mqttAlert.deviceId,
        deviceId:   mqttAlert.deviceId,
      );
    }

    // Ir para a aba de alertas automaticamente
    _tabController.animateTo(1);
  }

  Future<void> _loadDevices() async {
    setState(() => _isLoadingDevices = true);

    final result = await ApiService.getDevices();

    setState(() => _isLoadingDevices = false);

    if (!mounted) return;

    if (result['success']) {
      final devicesMap = result['data'] as Map<String, dynamic>;
      final loadedDevices = <Device>[];

      devicesMap.forEach((key, deviceData) {
        loadedDevices.add(
          Device(
            id: deviceData['device_id'].toString(),
            name: deviceData['nickname'],
            deviceId: deviceData['device_id'] is int
                ? deviceData['device_id'] as int
                : int.parse(deviceData['device_id'].toString()),
            dateAdded: DateTime.now(),
          ),
        );
      });

      setState(() => devices = loadedDevices);
      // Subscrever tópicos MQTT para cada dispositivo carregado
      for (final d in loadedDevices) {
        widget.mqttService.subscribeDevice(d.deviceId.toString());
      }
      // Atualizar foreground service com lista atual
      final ids   = loadedDevices.map((d) => d.deviceId.toString()).toList();
      final names = {for (final d in loadedDevices)
          d.deviceId.toString(): d.name};
      if (ids.isNotEmpty)
        startForegroundService(_mqttConfig, ids, deviceNames: names);
    } else {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(result['message']),
          backgroundColor: Colors.red,
        ),
      );
    }
  }

  Future<void> _logout() async {
    await stopForegroundService();
    final result = await ApiService.logout();
    if (mounted) {
      Navigator.of(context).pushReplacementNamed('/');
    }
  }

  Future<void> _deleteAccount() async {
    // Solicitar confirmação
    final shouldDelete = await showDialog<bool>(
      context: context,
      builder: (BuildContext context) => AlertDialog(
        title: const Text('Confirmar exclusão de conta'),
        content: const Text(
          'Esta ação é irreversível. Sua conta e todos os dados associados serão deletados permanentemente. Tem certeza?',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('Deletar', style: TextStyle(color: Colors.red)),
          ),
        ],
      ),
    );

    if (shouldDelete != true) return;

    final result = await ApiService.deleteAccount();

    if (mounted) {
      if (result['success']) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text(result['message'])),
        );
        // Retornar à tela inicial
        Navigator.of(context).pushReplacementNamed('/');
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(result['message']),
            backgroundColor: Colors.red,
          ),
        );
      }
    }
  }

  Future<void> _showEditAccountDialog() async {
    showDialog(
      context: context,
      builder: (context) => EditAccountDialog(
        currentUsername: widget.username,
        onSave: (newUsername, newPassword) async {
          final result = await ApiService.updateAccount(newUsername, newPassword);

          if (mounted) {
            if (result['success']) {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(content: Text(result['message'])),
              );
              // Fechar o dialog de edição
              Navigator.of(context).pop();
              // Mostrar snackbar de sucesso
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Dados da conta atualizados com sucesso!'),
                  backgroundColor: Colors.green,
                ),
              );
            } else {
              ScaffoldMessenger.of(context).showSnackBar(
                SnackBar(
                  content: Text(result['message']),
                  backgroundColor: Colors.red,
                ),
              );
            }
          }
        },
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          'Gerenciador SOS',
          style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.white),
        ),
        backgroundColor: const Color(0xFF1a3a52),
        elevation: 2,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back, color: Colors.white),
          onPressed: _safeBack,
        ),
        actions: [
          IconButton(
            icon: const Icon(Icons.edit, color: Colors.white),
            onPressed: _showEditAccountDialog,
            tooltip: 'Editar Conta',
          ),
          IconButton(
            icon: const Icon(Icons.settings_input_antenna, color: Colors.white),
            tooltip: 'Configurar MQTT',
            onPressed: _openMqttSettings,
          ),
          IconButton(
            icon: const Icon(Icons.delete_forever, color: Colors.red),
            onPressed: _deleteAccount,
            tooltip: 'Deletar Conta',
          ),
          IconButton(
            icon: const Icon(Icons.logout, color: Colors.white),
            onPressed: _logout,
            tooltip: 'Logout',
          ),
        ],
        bottom: TabBar(
          controller: _tabController,
          tabs: const [
            Tab(
              text: 'Aparelhos',
              icon: Icon(Icons.devices),
              iconMargin: EdgeInsets.only(bottom: 6),
            ),
            Tab(
              text: 'Alertas',
              icon: Icon(Icons.notifications),
              iconMargin: EdgeInsets.only(bottom: 6),
            ),
            Tab(
              text: 'MQTT',
              icon: Icon(Icons.hub),
              iconMargin: EdgeInsets.only(bottom: 6),
            ),
          ],
        ),
      ),
      body: TabBarView(
        controller: _tabController,
        children: [
          // Aba de Aparelhos
          _buildDevicesTab(),
          // Aba de Alertas
          _buildAlertsTab(),
          // Aba MQTT
          _buildMqttTab(),
        ],
      ),
      floatingActionButton: _tabController.index == 0
          ? FloatingActionButton(
              onPressed: () => _showAddEditDialog(null),
              backgroundColor: const Color(0xFF1a3a52),
              child: const Icon(Icons.add),
            )
          : _tabController.index == 1
              ? FloatingActionButton(
                  onPressed: _clearAlerts,
                  backgroundColor: Colors.red,
                  tooltip: 'Limpar alertas',
                  child: const Icon(Icons.delete_sweep),
                )
              : null,
    );
  }

  Widget _buildDevicesTab() {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(16.0),
          child: Card(
            elevation: 2,
            child: Padding(
              padding: const EdgeInsets.all(12.0),
              child: Text(
                'Usuário: ${widget.username}',
                style: const TextStyle(fontSize: 14, color: Color(0xFF1a3a52)),
              ),
            ),
          ),
        ),
        Expanded(
          child: _isLoadingDevices
              ? const Center(
                  child: CircularProgressIndicator(),
                )
              : devices.isEmpty
                  ? const Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          Icon(Icons.devices_other, size: 64, color: Colors.grey),
                          SizedBox(height: 16),
                          Text(
                            'Nenhum aparelho cadastrado',
                            style: TextStyle(fontSize: 16, color: Colors.grey),
                          ),
                          SizedBox(height: 8),
                          Text(
                            'Clique no botão + para adicionar um novo',
                            style: TextStyle(fontSize: 14, color: Colors.grey),
                          ),
                        ],
                      ),
                    )
                  : ListView.builder(
                      padding: const EdgeInsets.all(8.0),
                      itemCount: devices.length,
                  itemBuilder: (context, index) {
                    final device = devices[index];
                    return Card(
                      margin: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 8.0),
                      elevation: 3,
                      child: ListTile(
                        contentPadding: const EdgeInsets.all(16.0),
                        leading: const Icon(Icons.devices, color: Color(0xFF1a3a52), size: 32),
                        title: Text(
                          device.name,
                          style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 16),
                        ),
                        subtitle: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            const SizedBox(height: 8),
                            Text('Device ID: ${device.deviceId.toString()}'),
                            Text('Adicionado em: ${device.dateAdded.day}/${device.dateAdded.month}/${device.dateAdded.year}'),
                          ],
                        ),
                        trailing: SizedBox(
                          width: 100,
                          child: Row(
                            mainAxisAlignment: MainAxisAlignment.end,
                            children: [
                              IconButton(
                                icon: const Icon(Icons.edit, color: Colors.blue),
                                onPressed: () => _showAddEditDialog(device),
                              ),
                              IconButton(
                                icon: const Icon(Icons.delete, color: Colors.red),
                                onPressed: () => _deleteDevice(device),
                              ),
                            ],
                          ),
                        ),
                      ),
                    );
                  },
                ),
        ),
      ],
    );
  }

  Widget _buildMqttTab() {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: StreamBuilder<MqttConnState>(
            stream: widget.mqttService.connStateStream,
            initialData: widget.mqttService.connState,
            builder: (_, snap) {
              final state = snap.data ?? MqttConnState.disconnected;
              final (color, label, icon) = switch (state) {
                MqttConnState.connected    => (Colors.green,  'Conectado',    Icons.check_circle),
                MqttConnState.connecting   => (Colors.orange, 'Conectando…',  Icons.sync),
                MqttConnState.error        => (Colors.red,    'Erro',         Icons.error),
                MqttConnState.disconnected => (Colors.grey,   'Desconectado', Icons.cloud_off),
              };
              return Card(
                color: color.withOpacity(0.08),
                shape: RoundedRectangleBorder(
                  side: BorderSide(color: color.withOpacity(0.4)),
                  borderRadius: BorderRadius.circular(8),
                ),
                child: ListTile(
                  leading: Icon(icon, color: color),
                  title: Text('Broker: ${_mqttConfig.host}:${_mqttConfig.port}',
                      style: const TextStyle(fontWeight: FontWeight.bold)),
                  subtitle: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text('Status: $label',
                          style: TextStyle(color: color, fontWeight: FontWeight.w600)),
                      Text(
                        'TLS: ${_mqttConfig.useTls ? "sim" : "não"}  '
                        'Usuário: ${_mqttConfig.username}',
                        style: const TextStyle(fontSize: 12),
                      ),
                      if (widget.mqttService.subscribedDeviceIds.isNotEmpty)
                        Text(
                          'Monitorando: ${widget.mqttService.subscribedDeviceIds.join(", ")}',
                          style: const TextStyle(fontSize: 12),
                        ),
                    ],
                  ),
                  trailing: IconButton(
                    icon: const Icon(Icons.settings),
                    tooltip: 'Configurar',
                    onPressed: _openMqttSettings,
                  ),
                ),
              );
            },
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12),
          child: Row(
            children: [
              const Text('Log de mensagens',
                  style: TextStyle(fontWeight: FontWeight.bold,
                      color: Color(0xFF1a3a52))),
              const Spacer(),
              TextButton.icon(
                onPressed: () => setState(() => _mqttLog.clear()),
                icon: const Icon(Icons.delete_sweep, size: 18),
                label: const Text('Limpar'),
              ),
            ],
          ),
        ),
        Expanded(
          child: _mqttLog.isEmpty
              ? const Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(Icons.hub, size: 48, color: Colors.grey),
                      SizedBox(height: 12),
                      Text('Nenhuma mensagem ainda.',
                          style: TextStyle(color: Colors.grey)),
                      SizedBox(height: 6),
                      Text('Configure o broker e adicione dispositivos.',
                          style: TextStyle(fontSize: 12, color: Colors.grey)),
                    ],
                  ),
                )
              : ListView.builder(
                  padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
                  itemCount: _mqttLog.length,
                  itemBuilder: (_, i) => Padding(
                    padding: const EdgeInsets.symmetric(vertical: 2),
                    child: Text(
                      _mqttLog[i],
                      style: TextStyle(
                        fontFamily: 'monospace',
                        fontSize: 11,
                        color: _mqttLog[i].startsWith('←')
                            ? Colors.green.shade700
                            : _mqttLog[i].startsWith('Erro')
                                ? Colors.red
                                : Colors.grey.shade700,
                      ),
                    ),
                  ),
                ),
        ),
      ],
    );
  }

  Widget _buildAlertsTab() {
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(12.0),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceBetween,
            children: [
              Text(
                'Total de alertas: ${alerts.length}',
                style: const TextStyle(fontSize: 14, fontWeight: FontWeight.bold, color: Color(0xFF1a3a52)),
              ),
              if (alerts.isNotEmpty)
                TextButton.icon(
                  onPressed: () {
                    setState(() => alerts.clear());
                    ScaffoldMessenger.of(context).showSnackBar(
                      const SnackBar(
                        content: Text('Alertas limpos'),
                        backgroundColor: Colors.orange,
                      ),
                    );
                  },
                  icon: const Icon(Icons.delete_sweep, color: Colors.red),
                  label: const Text('Limpar', style: TextStyle(color: Colors.red)),
                ),
            ],
          ),
        ),
        Expanded(
          child: alerts.isEmpty
              ? const Center(
                  child: Column(
                    mainAxisAlignment: MainAxisAlignment.center,
                    children: [
                      Icon(Icons.notifications_none, size: 64, color: Colors.grey),
                      SizedBox(height: 16),
                      Text(
                        'Nenhum alerta recebido',
                        style: TextStyle(fontSize: 16, color: Colors.grey),
                      ),
                      SizedBox(height: 8),
                      Text(
                        'Os alertas dos seus aparelhos aparecerão aqui',
                        style: TextStyle(fontSize: 14, color: Colors.grey),
                      ),
                    ],
                  ),
                )
              : ListView.builder(
                  padding: const EdgeInsets.all(8.0),
                  itemCount: alerts.length,
                  itemBuilder: (context, index) {
                    final alert = alerts[index];
                    final color = _getAlertColor(alert.severity);
                    final icon = _getAlertIcon(alert.severity);

                    return Card(
                      margin: const EdgeInsets.symmetric(vertical: 8.0, horizontal: 8.0),
                      elevation: 2,
                      color: color.withOpacity(0.1),
                      child: ListTile(
                        contentPadding: const EdgeInsets.all(16.0),
                        leading: Icon(icon, color: color, size: 32),
                        title: Text(
                          alert.title,
                          style: TextStyle(fontWeight: FontWeight.bold, fontSize: 16, color: color),
                        ),
                        subtitle: Column(
                          crossAxisAlignment: CrossAxisAlignment.start,
                          children: [
                            const SizedBox(height: 8),
                            Text(alert.message),
                            const SizedBox(height: 6),
                            Text('Dispositivo: ${alert.deviceName}', style: const TextStyle(fontSize: 12)),
                            Text(
                              'Hora: ${alert.timestamp.hour}:${alert.timestamp.minute.toString().padLeft(2, '0')}',
                              style: const TextStyle(fontSize: 12, color: Colors.grey),
                            ),
                          ],
                        ),
                        trailing: IconButton(
                          icon: const Icon(Icons.close, color: Colors.grey),
                          onPressed: () {
                            setState(() => alerts.removeAt(index));
                          },
                        ),
                      ),
                    );
                  },
                ),
        ),
      ],
    );
  }

  Color _getAlertColor(AlertSeverity severity) {
    switch (severity) {
      case AlertSeverity.info:
        return Colors.blue;
      case AlertSeverity.warning:
        return Colors.orange;
      case AlertSeverity.error:
        return Colors.red;
    }
  }

  IconData _getAlertIcon(AlertSeverity severity) {
    switch (severity) {
      case AlertSeverity.info:
        return Icons.info;
      case AlertSeverity.warning:
        return Icons.warning;
      case AlertSeverity.error:
        return Icons.error;
    }
  }


  Future<void> _startForegroundService() async {
    await Future.delayed(const Duration(seconds: 2));
    if (!mounted) return;
    final ids   = devices.map((d) => d.deviceId.toString()).toList();
    final names = {for (final d in devices)
        d.deviceId.toString(): d.name};
    if (ids.isEmpty) return;
    await startForegroundService(_mqttConfig, ids, deviceNames: names);
  }

  Future<void> _safeBack() async {
    // Cancelar subscriptions antes de fechar para evitar setState após dispose
    await _alertSub.cancel();
    await _logSub.cancel();
    await widget.mqttService.disconnect();
    if (mounted) Navigator.of(context).pop();
  }

  void _clearAlerts() => setState(() => alerts.clear());

  Future<void> _openMqttSettings() async {
    final updated = await Navigator.of(context).push<MqttConfig>(
      MaterialPageRoute(
        builder: (_) => MqttSettingsScreen(
          mqttService:   widget.mqttService,
          currentConfig: _mqttConfig,
          deviceIds:     devices.map((d) => d.deviceId.toString()).toList(),
        ),
      ),
    );
    if (updated != null && mounted) {
      setState(() => _mqttConfig = updated);
    }
  }

  void _showAddEditDialog(Device? device) {
    if (device == null) {
      // ── NOVO DISPOSITIVO: coletar apelido e ir para ProvisionScreen ────────
      _showNicknameDialogThenProvision();
    } else {
      // ── EDITAR dispositivo existente: manter fluxo original ───────────────
      showDialog(
        context: context,
        builder: (context) => AddEditDeviceDialog(
          device: device,
          onSave: (name, deviceId) async {
            setState(() {
              final index = devices.indexWhere((d) => d.id == device.id);
              if (index != -1) {
                devices[index] = devices[index].copyWith(
                    name: name,
                    deviceId: int.tryParse(deviceId) ?? devices[index].deviceId);
              }
            });
            if (mounted) {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Aparelho atualizado!'),
                  backgroundColor: Colors.green,
                ),
              );
              Navigator.of(context).pop();
            }
          },
        ),
      );
    }
  }

  // Pede apelido e navega para ProvisionScreen; ao voltar salva via API
  Future<void> _showNicknameDialogThenProvision() async {
    final nicknameController = TextEditingController();
    final nickname = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Nome do aparelho'),
        content: TextField(
          controller: nicknameController,
          autofocus: true,
          decoration: InputDecoration(
            labelText: 'Ex: Relógio da Vovó',
            prefixIcon: const Icon(Icons.devices),
            border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(null),
            child: const Text('Cancelar'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1a3a52)),
            onPressed: () {
              final text = nicknameController.text.trim();
              if (text.isEmpty) return;
              Navigator.of(ctx).pop(text);
            },
            child: const Text('Próximo',
                style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );

    if (nickname == null || !mounted) return;

    // 1. Registrar no backend PRIMEIRO para obter o device_id inteiro
    final result = await ApiService.addDevice(nickname);
    if (!mounted) return;

    if (!result['success']) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(result['message']),
          backgroundColor: Colors.red,
        ),
      );
      return;
    }

    final backendDeviceId = result['device_id'];
    final int deviceIdInt = backendDeviceId is int
        ? backendDeviceId
        : int.tryParse(backendDeviceId.toString()) ?? 0;

    // 2. Navegar para tela BLE passando o device_id do backend
    //    O ESP32 receberá esse ID via GATT e o gravará em NVS
    final confirmed = await Navigator.of(context).push<String>(
      MaterialPageRoute(
        builder: (_) => ProvisionScreen(
          deviceNickname: nickname,
          deviceId: deviceIdInt,
        ),
      ),
    );

    if (!mounted) return;
    if (confirmed != null) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('$nickname configurado (ID: $deviceIdInt)!'),
          backgroundColor: Colors.green,
        ),
      );
    }
    _loadDevices();
  }


  void _deleteDevice(Device device) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Remover Aparelho'),
        content: Text(
            'Remover ${device.name} (ID: ${device.deviceId})? '
            'Isso ira remover o aparelho da sua conta e '
            'apagar as configuracoes salvas no aparelho.'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('Cancelar'),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(backgroundColor: Colors.red),
            onPressed: () async {
              Navigator.of(ctx).pop();
              await _doDeleteDevice(device);
            },
            child: const Text('Remover', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
  }


  Future<void> _doDeleteDevice(Device device) async {
    // 1. Remover do backend
    final result = await ApiService.deleteDevice(device.deviceId);
    if (!mounted) return;

    if (!result['success']) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text('Erro ao remover: ${result['message']}'),
          backgroundColor: Colors.red,
        ),
      );
      return;
    }

    // 2. Cancelar subscrição MQTT
    widget.mqttService.unsubscribeDevice(device.deviceId.toString());

    // 3. Atualizar lista local
    setState(() => devices.removeWhere((d) => d.id == device.id));

    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: Text('"${device.name}" removido.'),
        backgroundColor: Colors.red,
      ),
    );

    // 4. Oferecer reset BLE (opcional — device pode não estar acessível)
    if (!mounted) return;
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        content: const Text(
            'Para reutilizar o aparelho, abra a tela de configuração '
            'e use "Resetar provisionamento".'),
        duration: const Duration(seconds: 5),
        action: SnackBarAction(label: 'OK', onPressed: () {}),
      ),
    );
  }
}

// Dialog para adicionar/editar dispositivo
class AddEditDeviceDialog extends StatefulWidget {
  final Device? device;
  final Function(String name, String deviceId) onSave;

  const AddEditDeviceDialog({
    super.key,
    this.device,
    required this.onSave,
  });

  @override
  State<AddEditDeviceDialog> createState() => _AddEditDeviceDialogState();
}

class _AddEditDeviceDialogState extends State<AddEditDeviceDialog> {
  late TextEditingController _nameController;
  late TextEditingController _deviceIdController;
  final _formKey = GlobalKey<FormState>();

  @override
  void initState() {
    super.initState();
    _nameController = TextEditingController(text: widget.device?.name ?? '');
    _deviceIdController = TextEditingController(text: widget.device?.deviceId.toString() ?? '');
  }

  @override
  void dispose() {
    _nameController.dispose();
    _deviceIdController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: Text(widget.device != null ? 'Editar Aparelho' : 'Adicionar Aparelho'),
      content: Form(
        key: _formKey,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: _nameController,
                decoration: InputDecoration(
                  labelText: 'Nome do Aparelho',
                  hintText: 'Ex: Relógio SOS',
                  prefixIcon: const Icon(Icons.devices),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Por favor, digite o nome do aparelho';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: _deviceIdController,
                keyboardType: TextInputType.number,
                decoration: InputDecoration(
                  labelText: 'Device ID',
                  hintText: 'Ex: 42',
                  prefixIcon: const Icon(Icons.tag),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                  helperText: 'Numero inteiro atribuido pelo servidor',
                ),
                validator: (value) {
                  if (value == null || value.trim().isEmpty) {
                    return 'Digite o Device ID';
                  }
                  final parsed = int.tryParse(value.trim());
                  if (parsed == null) {
                    return 'Device ID deve ser um numero inteiro';
                  }
                  if (parsed <= 0) {
                    return 'Device ID deve ser maior que zero';
                  }
                  return null;
                },
              ),
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(),
          child: const Text('Cancelar'),
        ),
        ElevatedButton(
          style: ElevatedButton.styleFrom(
            backgroundColor: const Color(0xFF1a3a52),
          ),
          onPressed: () {
            if (_formKey.currentState!.validate()) {
              widget.onSave(
                _nameController.text,
                _deviceIdController.text,
              );
            }
          },
          child: const Text('Salvar', style: TextStyle(color: Colors.white)),
        ),
      ],
    );
  }
}

class ContatoPage extends StatelessWidget {
  const ContatoPage({super.key});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            const Text(
              'Entre em Contato',
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: Color(0xFF1a3a52),
              ),
            ),
            const SizedBox(height: 32),
            Card(
              elevation: 4,
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(8),
              ),
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  children: [
                    const ListTile(
                      leading: Icon(Icons.phone, color: Color(0xFF1a3a52)),
                      title: Text('Telefone'),
                      subtitle: Text('(11) 9999-9999'),
                    ),
                    const Divider(),
                    const ListTile(
                      leading: Icon(Icons.email, color: Color(0xFF1a3a52)),
                      title: Text('Email'),
                      subtitle: Text('contato@sosidoso.com'),
                    ),
                    const Divider(),
                    const ListTile(
                      leading: Icon(
                        Icons.location_on,
                        color: Color(0xFF1a3a52),
                      ),
                      title: Text('Endereço'),
                      subtitle: Text('São Paulo - SP'),
                    ),
                    const SizedBox(height: 16),
                    ElevatedButton.icon(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF1a3a52),
                      ),
                      onPressed: () {
                        ScaffoldMessenger.of(context).showSnackBar(
                          const SnackBar(content: Text('Mensagem enviada!')),
                        );
                      },
                      icon: const Icon(Icons.send),
                      label: const Text('Enviar Mensagem'),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class EditAccountDialog extends StatefulWidget {
  final Function(String username, String password) onSave;
  final String currentUsername;

  const EditAccountDialog({
    super.key,
    required this.onSave,
    required this.currentUsername,
  });

  @override
  State<EditAccountDialog> createState() => _EditAccountDialogState();
}

class _EditAccountDialogState extends State<EditAccountDialog> {
  late TextEditingController _usernameController;
  late TextEditingController _passwordController;
  late TextEditingController _confirmPasswordController;
  final _formKey = GlobalKey<FormState>();
  bool _obscurePassword = true;
  bool _obscureConfirmPassword = true;
  bool _isLoading = false;

  @override
  void initState() {
    super.initState();
    _usernameController = TextEditingController(text: widget.currentUsername);
    _passwordController = TextEditingController();
    _confirmPasswordController = TextEditingController();
  }

  @override
  void dispose() {
    _usernameController.dispose();
    _passwordController.dispose();
    _confirmPasswordController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return AlertDialog(
      title: const Text('Editar Dados da Conta'),
      content: Form(
        key: _formKey,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              TextFormField(
                controller: _usernameController,
                enabled: !_isLoading,
                decoration: InputDecoration(
                  labelText: 'Nome de Usuário',
                  hintText: 'Digite o novo nome de usuário',
                  prefixIcon: const Icon(Icons.person),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Por favor, digite o nome de usuário';
                  }
                  if (value.length < 3) {
                    return 'Nome de usuário deve ter pelo menos 3 caracteres';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: _passwordController,
                enabled: !_isLoading,
                obscureText: _obscurePassword,
                decoration: InputDecoration(
                  labelText: 'Nova Senha',
                  hintText: 'Digite a nova senha',
                  prefixIcon: const Icon(Icons.lock),
                  suffixIcon: IconButton(
                    icon: Icon(
                      _obscurePassword ? Icons.visibility_off : Icons.visibility,
                    ),
                    onPressed: () {
                      setState(() => _obscurePassword = !_obscurePassword);
                    },
                  ),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Por favor, digite a nova senha';
                  }
                  if (value.length < 6) {
                    return 'Senha deve ter pelo menos 6 caracteres';
                  }
                  return null;
                },
              ),
              const SizedBox(height: 16),
              TextFormField(
                controller: _confirmPasswordController,
                enabled: !_isLoading,
                obscureText: _obscureConfirmPassword,
                decoration: InputDecoration(
                  labelText: 'Confirmar Senha',
                  hintText: 'Confirme a nova senha',
                  prefixIcon: const Icon(Icons.lock),
                  suffixIcon: IconButton(
                    icon: Icon(
                      _obscureConfirmPassword ? Icons.visibility_off : Icons.visibility,
                    ),
                    onPressed: () {
                      setState(() => _obscureConfirmPassword = !_obscureConfirmPassword);
                    },
                  ),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Por favor, confirme a senha';
                  }
                  if (value != _passwordController.text) {
                    return 'As senhas não conferem';
                  }
                  return null;
                },
              ),
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: _isLoading ? null : () => Navigator.of(context).pop(),
          child: const Text('Cancelar'),
        ),
        ElevatedButton(
          style: ElevatedButton.styleFrom(
            backgroundColor: const Color(0xFF1a3a52),
          ),
          onPressed: _isLoading
              ? null
              : () async {
                  if (_formKey.currentState!.validate()) {
                    setState(() => _isLoading = true);
                    await widget.onSave(
                      _usernameController.text,
                      _passwordController.text,
                    );
                    setState(() => _isLoading = false);
                    if (mounted) {
                      Navigator.of(context).pop();
                    }
                  }
                },
          child: _isLoading
              ? const SizedBox(
                  height: 20,
                  width: 20,
                  child: CircularProgressIndicator(
                    strokeWidth: 2,
                    valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
                  ),
                )
              : const Text('Salvar', style: TextStyle(color: Colors.white)),
        ),
      ],
    );
  }
}

