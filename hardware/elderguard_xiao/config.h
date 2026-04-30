#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// ElderGuard — XIAO ESP32S3 (Seeed Studio)
// Hardware: MPU6500 IMU + MAX17048 fuel gauge, LiPo 350 mAh
// Comunicação: WiFi / MQTT + ESP-NOW serial mirror
// Sem: modem 4G, GPS, SD card
// ============================================================================

// ----------------------------------------------------------------------------
// I2C — BARRAMENTO ÚNICO compartilhado por MPU6500 e MAX17048
//
//   XIAO ESP32S3 pinos I2C padrão:
//     SDA = GPIO 5  (pino D4 / SDA na pinagem XIAO)
//     SCL = GPIO 6  (pino D5 / SCL na pinagem XIAO)
//
//   Endereços:
//     MPU6500  → 0x68  (AD0 = LOW)  ou 0x69 (AD0 = HIGH)
//     MAX17048 → 0x36  (fixo)
//
//   Ambos os chips são I2C 400 kHz (fast mode) e coexistem sem conflito.
// ----------------------------------------------------------------------------
#define I2C_SDA           5   // XIAO D4
#define I2C_SCL           6   // XIAO D5

// MPU6500
#define IMU_SDA           I2C_SDA
#define IMU_SCL           I2C_SCL
#define IMU_INT           3   // XIAO D2 — INT do MPU6500 → RTC GPIO, active LOW
                              // (GPIO 3 é RTC-capable no ESP32-S3)

// MAX17048 — mesmo barramento Wire que o IMU
#define MAX17048_I2C_ADDR     0x36
// Não usa Wire1 — usa Wire (mesmo bus do IMU)
#define MAX17048_SDA          I2C_SDA
#define MAX17048_SCL          I2C_SCL

// MAX17048 register map (idêntico ao original)
#define MAX17048_REG_VCELL    0x02
#define MAX17048_REG_SOC      0x04
#define MAX17048_REG_MODE     0x06
#define MAX17048_REG_VERSION  0x08
#define MAX17048_REG_CONFIG   0x0C
#define MAX17048_REG_CRATE    0x16
#define MAX17048_REG_VRESET   0x18
#define MAX17048_REG_STATUS   0x1A
#define MAX17048_REG_COMMAND  0xFE

// Botão de pânico
#define PANIC_BUTTON_PIN  0   // XIAO D0 — RTC GPIO, pull-up interno, press = LOW

// ----------------------------------------------------------------------------
// LED de alerta do botão de pânico (ativo em nível BAIXO)
//
//   GPIO 2 = XIAO D1 — RTC GPIO, saída digital
//   Circuito recomendado: GPIO2 → R(100Ω) → Cátodo do LED → GND
//   (o LED acende quando GPIO2 = LOW)
//
//   O LED permanece ACESO após qualquer evento de emergência (queda ou pânico)
//   e apaga somente após o responsável enviar "ack" via MQTT ou Dashboard.
//
//   Durante deep sleep, rtc_gpio_hold_en() mantém o pino no estado LOW
//   sem consumo adicional do núcleo principal — apenas o pull externo.
//   Consumo do LED aceso: depende do resistor escolhido.
//     R=100Ω  → (3.3V - Vf_LED) / 100Ω  ≈ 10–18 mA (LED vermelho típico)
//     R=220Ω  → ≈ 5–9 mA  (recomendado para LiPo pequena)
//     R=470Ω  → ≈ 2–4 mA  (mais econômico, ainda visível)
//   *** PARA MAXIMIZAR AUTONOMIA, USE R ≥ 330Ω ***
// ----------------------------------------------------------------------------
#define ALERT_LED_PIN     2   // XIAO D1 — active LOW (LED acende com GPIO LOW)
#define ALERT_LED_ON      LOW
#define ALERT_LED_OFF     HIGH

// ----------------------------------------------------------------------------
// TIMING — Heartbeat periódico
//   Carga normal  (SoC > CRITICAL_PCT): 15 minutos
//   Carga crítica (SoC ≤ CRITICAL_PCT): 30 minutos
// ----------------------------------------------------------------------------
#define SLEEP_INTERVAL_NORMAL_SEC    900   // 15 min
#define SLEEP_INTERVAL_CRITICAL_SEC  1800  // 30 min

// Mantido para compatibilidade com chamadas existentes de enterDeepSleep();
// o valor real é calculado em runtime pelo PowerManager::getSleepInterval().
#define SLEEP_INTERVAL_SEC           SLEEP_INTERVAL_NORMAL_SEC

// ----------------------------------------------------------------------------
// Bateria / fuel gauge
// ----------------------------------------------------------------------------
#define MAX17048_ALERT_THRESHOLD  10   // Alerta ao cruzar 10 % (1–32 %)
#define MAX17048_CRITICAL_PCT     10   // Abaixo → modo critical (30 min heartbeat)
#define MAX17048_SLEEP_WITH_ESP   true // Gauge dorme junto com o ESP32 (~0.5 µA)

// ----------------------------------------------------------------------------
// Detecção de queda — limites idênticos ao projeto original
// ----------------------------------------------------------------------------
#define FALL_ACCEL_THRESHOLD      30.0f
#define FALL_GYRO_THRESHOLD       250.0f
#define FALL_FREEFALL_THRESHOLD   2.0f
#define FALL_FREEFALL_DURATION_MS 80
#define WOM_THRESHOLD             150   // ~600 mg
#define FALL_CONFIRM_WINDOW_SEC   5

// ----------------------------------------------------------------------------
// WiFi + MQTT
// ----------------------------------------------------------------------------
#define WIFI_SSID           "YourSSID"
#define WIFI_PASSWORD       "YourPassword"

#define MQTT_BROKER         "iot.gtpc.com.br"
#define MQTT_PORT           1883
#define MQTT_USER           "elderguard"
#define MQTT_PASSWORD       "changeme"
#define MQTT_CLIENT_ID      "elderguard-xiao-001"
#define MQTT_KEEPALIVE_SEC  60
#define MQTT_CONNECT_TIMEOUT_MS  10000
#define MQTT_USE_TLS        false

#define MQTT_TOPIC_BASE     "elderguard/001"
#define MQTT_TOPIC_ALERT    MQTT_TOPIC_BASE "/alert"
#define MQTT_TOPIC_TELEM    MQTT_TOPIC_BASE "/telemetry"
#define MQTT_TOPIC_STATUS   MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_CMD      MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_DEBUG    MQTT_TOPIC_BASE "/debug"
// Acknowledge de alerta — publicar qualquer payload neste tópico apaga o LED
// Exemplo dashboard: mosquitto_pub -t elderguard/001/ack -m "ok" -r
#define MQTT_TOPIC_ACK      MQTT_TOPIC_BASE "/ack"

// ----------------------------------------------------------------------------
// ESP-NOW Serial Mirror
// Descomente a linha abaixo para desabilitar o mirror completamente
// (economiza ~1 mA de corrente média e ~10 KB de RAM)
// ----------------------------------------------------------------------------
// #define DISABLE_ESPNOW
#define ESPNOW_CHANNEL    0   // 0 = canal atual do WiFi; 1–13 = canal fixo

// ----------------------------------------------------------------------------
// BLE
// ----------------------------------------------------------------------------
#define BLE_DEVICE_NAME     "ElderGuard-XIAO-001"

// ----------------------------------------------------------------------------
// Alerta de emergência — sem SMS (sem modem); usa MQTT. Se offline,
// o alerta é armazenado em RTC RAM e reenviado no próximo ciclo.
// ----------------------------------------------------------------------------
#define ALERT_SERVER_URL    "http://iot.gtpc.com.br/api/alert"

// ----------------------------------------------------------------------------
// Wake-up reason
// ----------------------------------------------------------------------------
typedef enum {
    WAKE_UNKNOWN   = 0,
    WAKE_TIMER     = 1,
    WAKE_PANIC     = 2,
    WAKE_FALL_IMU  = 3,
} wake_reason_t;

#endif // CONFIG_H
