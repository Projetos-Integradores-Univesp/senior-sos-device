#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// ElderGuard — XIAO ESP32S3
// Hardware: MPU6500 IMU + MAX17048 fuel gauge, LiPo 350 mAh
// Comunicação: WiFi/MQTT (credenciais via BLE) + ESP-NOW mirror
//
// PROVISIONAMENTO VIA BLE:
//   WiFi SSID, senha e device ID NÃO são definidos aqui.
//   São gravados pelo app mobile via BLE GATT e persistidos em NVS.
//   Ao desprovisionar (reset de fábrica), o dispositivo volta ao modo BLE.
//
// CREDENCIAIS DO BROKER:
//   Nunca commitar valores reais. Injetar via variáveis de ambiente SECRET_*
//   (GitHub Secrets no CI ou .env local ignorado pelo .gitignore).
// ============================================================================

// ----------------------------------------------------------------------------
// I2C — MPU6500 + MAX17048
// ----------------------------------------------------------------------------
#define I2C_SDA           5   // XIAO D4
#define I2C_SCL           6   // XIAO D5
#define IMU_SDA           I2C_SDA
#define IMU_SCL           I2C_SCL
#define IMU_INT           3   // XIAO D2, RTC GPIO, active LOW
#define MAX17048_I2C_ADDR 0x36
#define MAX17048_SDA      I2C_SDA
#define MAX17048_SCL      I2C_SCL

// Registradores MAX17048
#define MAX17048_REG_VCELL   0x02
#define MAX17048_REG_SOC     0x04
#define MAX17048_REG_MODE    0x06
#define MAX17048_REG_VERSION 0x08
#define MAX17048_REG_CONFIG  0x0C
#define MAX17048_REG_CRATE   0x16
#define MAX17048_REG_VRESET  0x18
#define MAX17048_REG_STATUS  0x1A
#define MAX17048_REG_COMMAND 0xFE

// ----------------------------------------------------------------------------
// Botão de pânico e LED de alerta
// ----------------------------------------------------------------------------
#define PANIC_BUTTON_PIN  1   // XIAO D0 = GPIO1, RTC GPIO, pull-up, press = LOW
#define ALERT_LED_PIN     2   // XIAO D1, RTC GPIO, active LOW
#define ALERT_LED_ON      LOW
#define ALERT_LED_OFF     HIGH

// ----------------------------------------------------------------------------
// Broker MQTT
// Lido de variáveis de ambiente em tempo de compilação via platformio.ini.
//
// Defina antes de compilar (ou via GitHub Secrets no CI):
//   export SECRET_MQTT_BROKER=seu-broker.exemplo.com
//   export SECRET_MQTT_USERNAME=elderguard
//   export SECRET_MQTT_PASSWORD=sua-senha-segura
//
// O platformio.ini injeta essas variáveis como macros via build_flags.
// O compilador emitirá erro explícito se alguma estiver ausente.
// ----------------------------------------------------------------------------
#ifndef SECRET_MQTT_BROKER
  #error "SECRET_MQTT_BROKER não definido. Exporte a variável de ambiente antes de compilar."
#endif
#ifndef SECRET_MQTT_USERNAME
  #error "SECRET_MQTT_USERNAME não definido. Exporte a variável de ambiente antes de compilar."
#endif
#ifndef SECRET_MQTT_PASSWORD
  #error "SECRET_MQTT_PASSWORD não definido. Exporte a variável de ambiente antes de compilar."
#endif

#define MQTT_BROKER             SECRET_MQTT_BROKER
#define MQTT_PORT               1883
#define MQTT_USERNAME           SECRET_MQTT_USERNAME
#define MQTT_PASSWORD_BROKER    SECRET_MQTT_PASSWORD
#define MQTT_KEEPALIVE_SEC      60
#define MQTT_CONNECT_TIMEOUT_MS 10000
#define MQTT_USE_TLS            false

// ----------------------------------------------------------------------------
// Tópicos MQTT — construídos em runtime com o device ID (ver ProvisionManager)
//   devices/{id}/fall            ← queda detectada
//   devices/{id}/button-pressed  ← botão de pânico
//   devices/{id}/telemetry       ← heartbeat periódico
//   devices/{id}/status          ← online/offline (LWT)
// ----------------------------------------------------------------------------
// Prefixo e sufixos — concatenados com deviceId em runtime
#define MQTT_TOPIC_PREFIX       "devices/"
#define MQTT_TOPIC_FALL         "/fall"
#define MQTT_TOPIC_BUTTON       "/button-pressed"
#define MQTT_TOPIC_TELEMETRY    "/telemetry"
#define MQTT_TOPIC_STATUS       "/status"
#define MQTT_TOPIC_ACK          "/ack"

// Tamanho máximo dos tópicos (prefixo + ID + sufixo)
// Ex: "devices/elderguard-abc123/button-pressed" = 43 chars
#define MQTT_TOPIC_MAX_LEN      64

// ----------------------------------------------------------------------------
// NVS — chaves de persistência (namespace "elderguard")
// ----------------------------------------------------------------------------
#define NVS_NAMESPACE           "elderguard"
#define NVS_KEY_DEVICE_ID       "device_id"   // string, ex: "watch-a1b2c3"
#define NVS_KEY_WIFI_SSID       "wifi_ssid"   // string
#define NVS_KEY_WIFI_PASS       "wifi_pass"   // string
#define NVS_KEY_PROVISIONED     "provisioned" // uint8: 0=não, 1=sim

// ----------------------------------------------------------------------------
// BLE GATT — UUIDs do serviço de provisionamento
//
// Serviço de provisionamento:
//   SERVICE_PROV_UUID  → serviço principal
//   CHAR_SSID_UUID     → SSID WiFi        (WRITE)
//   CHAR_PASS_UUID     → Senha WiFi       (WRITE)
//   CHAR_ID_UUID       → Device ID        (WRITE | READ)
//   CHAR_STATUS_UUID   → Status/telemetria (READ | NOTIFY)
//   CHAR_CMD_UUID      → Comandos (ack)   (WRITE)
//
// O app escreve SSID, PASS e ID; o dispositivo persiste em NVS e reinicia.
// Se qualquer campo estiver em branco, o dispositivo permanece em modo BLE.
// ----------------------------------------------------------------------------
#define SERVICE_PROV_UUID   "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_SSID_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a0"
#define CHAR_PASS_UUID      "beb5483e-36e1-4688-b7f5-ea07361b26a1"
#define CHAR_ID_UUID        "beb5483e-36e1-4688-b7f5-ea07361b26a2"
#define CHAR_STATUS_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a3"
#define CHAR_CMD_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26a4"

// Nome BLE broadcast (app filtra por este nome)
#define BLE_ADV_NAME        "ElderGuard"

// ----------------------------------------------------------------------------
// TIMING
// ----------------------------------------------------------------------------
#define SLEEP_INTERVAL_NORMAL_SEC    900    // 15 min
#define SLEEP_INTERVAL_CRITICAL_SEC  1800   // 30 min
#define SLEEP_INTERVAL_SEC           SLEEP_INTERVAL_NORMAL_SEC
#define BLE_PROVISION_TIMEOUT_MS     120000 // 2 min aguardando app; 0=sem timeout

// ----------------------------------------------------------------------------
// Bateria
// ----------------------------------------------------------------------------
#define MAX17048_ALERT_THRESHOLD  10
#define MAX17048_CRITICAL_PCT     10
#define MAX17048_SLEEP_WITH_ESP   true

// ----------------------------------------------------------------------------
// Detecção de queda
// ----------------------------------------------------------------------------
#define FALL_ACCEL_THRESHOLD      30.0f
#define FALL_GYRO_THRESHOLD       250.0f
#define FALL_FREEFALL_THRESHOLD   2.0f
#define FALL_FREEFALL_DURATION_MS 80
#define WOM_THRESHOLD             150
#define FALL_CONFIRM_WINDOW_SEC   5

// ----------------------------------------------------------------------------
// ESP-NOW mirror
// ----------------------------------------------------------------------------
// #define DISABLE_ESPNOW
#define ESPNOW_CHANNEL    0

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
