import 'package:flutter/material.dart';

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
  const MyApp({Key? key}) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'SOS IDOSO',
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
  const IntroScreen({Key? key}) : super(key: key);

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
  const HomePage({Key? key}) : super(key: key);

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
  const LoginPage({Key? key}) : super(key: key);

  @override
  State<LoginPage> createState() => _LoginPageState();
}

class _LoginPageState extends State<LoginPage> {
  final _emailController = TextEditingController();
  final _senhaController = TextEditingController();

  @override
  void dispose() {
    _emailController.dispose();
    _senhaController.dispose();
    super.dispose();
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
              controller: _emailController,
              decoration: InputDecoration(
                labelText: 'Email',
                hintText: 'Digite seu email',
                prefixIcon: const Icon(Icons.email),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(8),
                ),
              ),
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _senhaController,
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
              onPressed: () {
                if (_emailController.text.isNotEmpty && _senhaController.text.isNotEmpty) {
                  // Simular login bem-sucedido
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('Fazendo login...')),
                  );
                  // Navegar para a página de gerenciamento de dispositivos
                  Navigator.of(context).push(
                    MaterialPageRoute(
                      builder: (_) => DeviceManagementPage(
                        userEmail: _emailController.text,
                      ),
                    ),
                  );
                } else {
                  ScaffoldMessenger.of(context).showSnackBar(
                    const SnackBar(content: Text('Por favor, preencha todos os campos')),
                  );
                }
              },
              child: const Text(
                'Entrar',
                style: TextStyle(fontSize: 16, color: Colors.white),
              ),
            ),
            const SizedBox(height: 12),
            TextButton(
              onPressed: () {
                Navigator.of(context).push(
                  MaterialPageRoute(builder: (_) => const RegistrationScreen()),
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
  const RegistrationScreen({Key? key}) : super(key: key);

  @override
  State<RegistrationScreen> createState() => _RegistrationScreenState();
}

class _RegistrationScreenState extends State<RegistrationScreen> {
  final _nameController = TextEditingController();
  final _emailController = TextEditingController();
  final _passwordController = TextEditingController();

  @override
  void dispose() {
    _nameController.dispose();
    _emailController.dispose();
    _passwordController.dispose();
    super.dispose();
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
              controller: _nameController,
              decoration: const InputDecoration(labelText: 'Nome'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _emailController,
              decoration: const InputDecoration(labelText: 'Email'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: _passwordController,
              obscureText: true,
              decoration: const InputDecoration(labelText: 'Senha'),
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1a3a52),
              ),
              onPressed: () {
                ScaffoldMessenger.of(context).showSnackBar(
                  const SnackBar(content: Text('Cadastro realizado!')),
                );
                Navigator.of(context).pop();
              },
              child: const Text('Cadastrar'),
            ),
          ],
        ),
      ),
    );
  }
}

// Página de Gerenciamento de Dispositivos
class DeviceManagementPage extends StatefulWidget {
  final String userEmail;

  const DeviceManagementPage({
    Key? key,
    required this.userEmail,
  }) : super(key: key);

  @override
  State<DeviceManagementPage> createState() => _DeviceManagementPageState();
}

class _DeviceManagementPageState extends State<DeviceManagementPage> {
  List<Device> devices = [];

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text(
          'Meus Aparelhos',
          style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.white),
        ),
        backgroundColor: const Color(0xFF1a3a52),
        elevation: 2,
        leading: IconButton(
          icon: const Icon(Icons.arrow_back, color: Colors.white),
          onPressed: () => Navigator.of(context).pop(),
        ),
      ),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16.0),
            child: Card(
              elevation: 2,
              child: Padding(
                padding: const EdgeInsets.all(12.0),
                child: Text(
                  'Usuário: ${widget.userEmail}',
                  style: const TextStyle(fontSize: 14, color: Color(0xFF1a3a52)),
                ),
              ),
            ),
          ),
          Expanded(
            child: devices.isEmpty
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
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showAddEditDialog(null),
        backgroundColor: const Color(0xFF1a3a52),
        child: const Icon(Icons.add),
      ),
    );
  }

  void _showAddEditDialog(Device? device) {
    showDialog(
      context: context,
      builder: (context) => AddEditDeviceDialog(
        device: device,
        onSave: (name, deviceId) {
          setState(() {
            if (device != null) {
              // Editar dispositivo existente
              final index = devices.indexWhere((d) => d.id == device.id);
              if (index != -1) {
                devices[index] = devices[index].copyWith(name: name, deviceId: deviceId);
              }
            } else {
              // Adicionar novo dispositivo
              devices.add(Device(
                id: DateTime.now().toString(),
                name: name,
                deviceId: deviceId,
                dateAdded: DateTime.now(),
              ));
            }
          });
          Navigator.of(context).pop();
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(device != null ? 'Aparelho atualizado!' : 'Aparelho adicionado!'),
              backgroundColor: Colors.green,
            ),
          );
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
    Key? key,
    this.device,
    required this.onSave,
  }) : super(key: key);

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
  const ContatoPage({Key? key}) : super(key: key);

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
