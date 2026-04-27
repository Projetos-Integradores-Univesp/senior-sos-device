import 'package:flutter/material.dart';
import 'services/api_service.dart';

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
  final String deviceId;
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
    String? deviceId,
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

void main() {
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
          builder: (_) => DeviceManagementPage(
            username: _usernameController.text,
          ),
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
  final String username;

  const DeviceManagementPage({
    super.key,
    required this.username,
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

  @override
  void initState() {
    super.initState();
    _tabController = TabController(length: 2, vsync: this);
    _loadDevices();
  }

  @override
  void dispose() {
    _tabController.dispose();
    super.dispose();
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
            deviceId: deviceData['device_id'].toString(),
            dateAdded: DateTime.now(),
          ),
        );
      });

      setState(() => devices = loadedDevices);
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
    final result = await ApiService.logout();
    if (mounted) {
      Navigator.of(context).pushReplacementNamed('/');
    }
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
          onPressed: () => Navigator.of(context).pop(),
        ),
        actions: [
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
        ],
      ),
      floatingActionButton: _tabController.index == 0
          ? FloatingActionButton(
              onPressed: () => _showAddEditDialog(null),
              backgroundColor: const Color(0xFF1a3a52),
              child: const Icon(Icons.add),
            )
          : FloatingActionButton(
              onPressed: () => _simulateAlert(),
              backgroundColor: const Color(0xFF1a3a52),
              tooltip: 'Simular alerta',
              child: const Icon(Icons.notifications_active),
            ),
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
                            Text('Device ID: ${device.deviceId}'),
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
                                onPressed: () => _deleteDevice(device.id),
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

  void _simulateAlert() {
    if (devices.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
          content: Text('Adicione um aparelho primeiro'),
          backgroundColor: Colors.orange,
        ),
      );
      return;
    }

    final severities = [AlertSeverity.info, AlertSeverity.warning, AlertSeverity.error];
    final messages = [
      'Bateria baixa',
      'SOS acionado',
      'Conexão perdida',
      'Localização atualizada',
      'Modo de emergência ativado',
      'Sensor de queda detectado',
    ];

    setState(() {
      alerts.insert(
        0,
        Alert(
          id: DateTime.now().toString(),
          title: messages[(messages.length * DateTime.now().microsecond) ~/ 999999],
          message: 'Ação necessária no seu aparelho',
          timestamp: DateTime.now(),
          deviceName: devices[(devices.length * DateTime.now().microsecond) ~/ 999999].name,
          severity: severities[(severities.length * DateTime.now().microsecond) ~/ 999999],
        ),
      );
    });

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(
        content: Text('Novo alerta recebido!'),
        backgroundColor: Colors.blue,
      ),
    );
  }

  void _showAddEditDialog(Device? device) {
    showDialog(
      context: context,
      builder: (context) => AddEditDeviceDialog(
        device: device,
        onSave: (name, deviceId) async {
          if (device == null) {
            // Adicionar novo dispositivo via API
            final result = await ApiService.addDevice(name);
            if (mounted) {
              if (result['success']) {
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text(result['message']),
                    backgroundColor: Colors.green,
                  ),
                );
                // Recarregar dispositivos
                _loadDevices();
              } else {
                ScaffoldMessenger.of(context).showSnackBar(
                  SnackBar(
                    content: Text(result['message']),
                    backgroundColor: Colors.red,
                  ),
                );
              }
            }
          } else {
            // Editar dispositivo existente (local por enquanto)
            setState(() {
              final index = devices.indexWhere((d) => d.id == device.id);
              if (index != -1) {
                devices[index] = devices[index].copyWith(name: name, deviceId: deviceId);
              }
            });
            if (mounted) {
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Aparelho atualizado!'),
                  backgroundColor: Colors.green,
                ),
              );
            }
          }
          if (mounted) {
            Navigator.of(context).pop();
          }
        },
      ),
    );
  }

  void _deleteDevice(String deviceId) {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Remover Aparelho'),
        content: const Text('Tem certeza que deseja remover este aparelho?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(),
            child: const Text('Cancelar'),
          ),
          TextButton(
            onPressed: () {
              setState(() {
                devices.removeWhere((d) => d.id == deviceId);
              });
              Navigator.of(context).pop();
              ScaffoldMessenger.of(context).showSnackBar(
                const SnackBar(
                  content: Text('Aparelho removido!'),
                  backgroundColor: Colors.red,
                ),
              );
            },
            child: const Text('Remover', style: TextStyle(color: Colors.red)),
          ),
        ],
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
    _deviceIdController = TextEditingController(text: widget.device?.deviceId ?? '');
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
                decoration: InputDecoration(
                  labelText: 'Device ID',
                  hintText: 'Ex: DEV-12345-ABCDE',
                  prefixIcon: const Icon(Icons.qr_code),
                  border: OutlineInputBorder(borderRadius: BorderRadius.circular(8)),
                ),
                validator: (value) {
                  if (value == null || value.isEmpty) {
                    return 'Por favor, digite o Device ID';
                  }
                  if (value.length < 5) {
                    return 'Device ID deve ter pelo menos 5 caracteres';
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
