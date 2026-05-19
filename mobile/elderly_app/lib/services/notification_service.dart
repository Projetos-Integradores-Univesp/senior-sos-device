import 'package:flutter_local_notifications/flutter_local_notifications.dart';
import 'package:permission_handler/permission_handler.dart';

// ============================================================================
// NotificationService — notificações do sistema para alertas ElderGuard
// ============================================================================
class NotificationService {
  static final _plugin = FlutterLocalNotificationsPlugin();
  static bool _initialized = false;
  static int  _nextId = 0;

  // Canais Android
  static const _channelFall = AndroidNotificationChannel(
    'elderguard_fall',
    'Quedas',
    description: 'Alertas de queda detectada pelo ElderGuard',
    importance: Importance.max,
    playSound: true,
    enableVibration: true,
  );
  static const _channelButton = AndroidNotificationChannel(
    'elderguard_button',
    'Botão de Pânico',
    description: 'Alertas do botão de pânico do ElderGuard',
    importance: Importance.max,
    playSound: true,
    enableVibration: true,
  );

  // ── Inicializar ─────────────────────────────────────────────────────────
  static Future<void> init() async {
    if (_initialized) return;

    const android = AndroidInitializationSettings('@mipmap/ic_launcher');
    const ios     = DarwinInitializationSettings(
      requestAlertPermission: true,
      requestBadgePermission: true,
      requestSoundPermission: true,
    );
    await _plugin.initialize(
      const InitializationSettings(android: android, iOS: ios),
    );

    // Criar canais Android
    final androidPlugin = _plugin
        .resolvePlatformSpecificImplementation<
            AndroidFlutterLocalNotificationsPlugin>();
    await androidPlugin?.createNotificationChannel(_channelFall);
    await androidPlugin?.createNotificationChannel(_channelButton);

    _initialized = true;
  }

  // ── Solicitar permissão (Android 13+) ────────────────────────────────────
  static Future<bool> requestPermission() async {
    // flutter_local_notifications >= 14 solicita via própria API no iOS
    // No Android 13+ (API 33+) precisamos da permissão POST_NOTIFICATIONS
    final status = await Permission.notification.request();
    return status.isGranted;
  }

  // ── Mostrar notificação de queda ─────────────────────────────────────────
  static Future<void> showFallAlert({
    required String deviceName,
    required String deviceId,
    String? extra,
  }) async {
    if (!_initialized) await init();
    await _plugin.show(
      _nextId++,
      '⚠️ Queda detectada',
      '$deviceName (ID: $deviceId)${extra != null ? " — $extra" : ""}',
      NotificationDetails(
        android: AndroidNotificationDetails(
          _channelFall.id,
          _channelFall.name,
          channelDescription: _channelFall.description,
          importance: Importance.max,
          priority: Priority.high,
          ticker: 'Queda detectada',
          styleInformation: BigTextStyleInformation(
            '$deviceName (ID: $deviceId)\n${extra ?? ""}',
            summaryText: 'ElderGuard',
          ),
        ),
        iOS: const DarwinNotificationDetails(
          presentAlert: true,
          presentBadge: true,
          presentSound: true,
        ),
      ),
    );
  }

  // ── Mostrar notificação de botão de pânico ───────────────────────────────
  static Future<void> showPanicAlert({
    required String deviceName,
    required String deviceId,
  }) async {
    if (!_initialized) await init();
    await _plugin.show(
      _nextId++,
      '🆘 Botão de pânico',
      '$deviceName (ID: $deviceId) acionou o botão de emergência!',
      NotificationDetails(
        android: AndroidNotificationDetails(
          _channelButton.id,
          _channelButton.name,
          channelDescription: _channelButton.description,
          importance: Importance.max,
          priority: Priority.high,
          ticker: 'Botão de pânico',
          styleInformation: BigTextStyleInformation(
            '$deviceName (ID: $deviceId)\nBotão de pânico acionado!',
            summaryText: 'ElderGuard',
          ),
        ),
        iOS: const DarwinNotificationDetails(
          presentAlert: true,
          presentBadge: true,
          presentSound: true,
        ),
      ),
    );
  }
}
