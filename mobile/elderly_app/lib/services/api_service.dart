import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'dart:convert';

class ApiService {
  static const String baseUrl = 'http://127.0.0.1:8000';
  static const String loginEndpoint = '$baseUrl/auth/login';
  static const String registerEndpoint = '$baseUrl/users/';
  static const String devicesEndpoint = '$baseUrl/devices/';
  static const String logoutEndpoint = '$baseUrl/auth/logout';

  // Fazer login
  static Future<Map<String, dynamic>> login(
      String username, String password) async {
    try {
      final response = await http.post(
        Uri.parse(loginEndpoint),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'username': username,
          'password': password,
        }),
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        final detail = data['detail'];

        // Armazenar tokens localmente
        await _saveTokens(
          detail['access_token'],
          detail['refresh_token'],
          username,
        );

        return {
          'success': true,
          'message': detail['message'],
          'access_token': detail['access_token'],
          'refresh_token': detail['refresh_token'],
        };
      } else {
        final errorData = jsonDecode(response.body);
        return {
          'success': false,
          'message': errorData['detail']['message'] ?? 'Erro ao fazer login',
        };
      }
    } catch (e) {
      return {
        'success': false,
        'message': 'Erro de conexão: $e',
      };
    }
  }

  // Registrar novo usuário
  static Future<Map<String, dynamic>> register(
      String username, String password) async {
    try {
      final response = await http.post(
        Uri.parse(registerEndpoint),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({
          'username': username,
          'password': password,
        }),
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return {
          'success': true,
          'message': data['detail']['message'] ?? 'Usuário registrado com sucesso',
        };
      } else {
        final errorData = jsonDecode(response.body);
        return {
          'success': false,
          'message': errorData['detail']['message'] ?? 'Erro ao registrar',
        };
      }
    } catch (e) {
      return {
        'success': false,
        'message': 'Erro de conexão: $e',
      };
    }
  }

  // Listar dispositivos do usuário
  static Future<Map<String, dynamic>> getDevices() async {
    try {
      final token = await _getAccessToken();

      if (token == null) {
        return {
          'success': false,
          'message': 'Token não encontrado. Faça login novamente.',
        };
      }

      final response = await http.get(
        Uri.parse(devicesEndpoint),
        headers: {
          'Authorization': 'Bearer $token',
          'Content-Type': 'application/json',
        },
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return {
          'success': true,
          'data': data,
        };
      } else {
        return {
          'success': false,
          'message': 'Erro ao listar dispositivos',
        };
      }
    } catch (e) {
      return {
        'success': false,
        'message': 'Erro de conexão: $e',
      };
    }
  }

  // Adicionar novo dispositivo
  static Future<Map<String, dynamic>> addDevice(String nickname) async {
    try {
      final token = await _getAccessToken();

      if (token == null) {
        return {
          'success': false,
          'message': 'Token não encontrado. Faça login novamente.',
        };
      }

      final response = await http.post(
        Uri.parse('$devicesEndpoint?nickname=$nickname'),
        headers: {
          'Authorization': 'Bearer $token',
          'Content-Type': 'application/json',
        },
      );

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return {
          'success': true,
          'message': data['detail']['message'] ?? 'Dispositivo adicionado com sucesso',
          'device_id': data['detail']['device_id'],
        };
      } else {
        final errorData = jsonDecode(response.body);
        return {
          'success': false,
          'message':
              errorData['detail']['message'] ?? 'Erro ao adicionar dispositivo',
        };
      }
    } catch (e) {
      return {
        'success': false,
        'message': 'Erro de conexão: $e',
      };
    }
  }

  // Fazer logout
  static Future<Map<String, dynamic>> logout() async {
    try {
      final token = await _getAccessToken();

      if (token == null) {
        await _clearTokens();
        return {
          'success': true,
          'message': 'Logout realizado',
        };
      }

      await http.post(
        Uri.parse(logoutEndpoint),
        headers: {
          'Authorization': 'Bearer $token',
          'Content-Type': 'application/json',
        },
      );

      await _clearTokens();

      return {
        'success': true,
        'message': 'Logout realizado com sucesso',
      };
    } catch (e) {
      await _clearTokens();
      return {
        'success': true,
        'message': 'Logout realizado',
      };
    }
  }

  // Salvar tokens localmente
  static Future<void> _saveTokens(
      String accessToken, String refreshToken, String username) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('access_token', accessToken);
    await prefs.setString('refresh_token', refreshToken);
    await prefs.setString('username', username);
  }

  // Obter access token armazenado
  static Future<String?> _getAccessToken() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString('access_token');
  }

  // Obter username armazenado
  static Future<String?> getStoredUsername() async {
    final prefs = await SharedPreferences.getInstance();
    return prefs.getString('username');
  }

  // Limpar tokens (logout)
  static Future<void> _clearTokens() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('access_token');
    await prefs.remove('refresh_token');
    await prefs.remove('username');
  }

  // Verificar se usuário está autenticado
  static Future<bool> isAuthenticated() async {
    final token = await _getAccessToken();
    return token != null;
  }
}
