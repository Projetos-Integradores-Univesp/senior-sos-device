#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PIN DEFINITIONS — ESP32-S3-A7670E-4G + BNO086
// ============================================================================

// BNO086 IMU (I2C)
#define BNO086_SDA        2
#define BNO086_SCL        1
#define BNO086_INT        3   // Interrupt → deep-sleep ext0 wake source
#define BNO086_RST        47  // Hardware reset — MUST NOT conflict with SD pins
                              // GPIO 4 is SD_CMD! Rewire RST to GPIO 47.
#define BNO086_I2C_ADDR   0x4B  // Confirmed via diagnostic (SA0 is HIGH on this board)

// Panic Button
#define PANIC_BUTTON_PIN  0   // RTC GPIO, pulled HIGH internally, press = LOW

// A7670E 4G Module (hard-wired on the dev board)
// CRITICAL: Serial1.begin(baud, config, rxPin, txPin)
//   rxPin = GPIO the ESP32 READS from (modem's TX line)
//   txPin = GPIO the ESP32 WRITES to  (modem's RX line)
#define A7670E_RX         17  // ESP32 RX ← Module TX (ESP reads from GPIO 17)
#define A7670E_TX         18  // ESP32 TX → Module RX (ESP writes to GPIO 18)
#define A7670E_PWR        12  // Module power key (check your board schematic)
#define A7670E_RST_PIN    5   // Module reset (check your board schematic)
#define A7670E_BAUD       115200

// Cellular APN — MUST match your SIM card's carrier
// Common Brazilian APNs:
//   Claro:  "claro.com.br"
//   Vivo:   "zap.vivo.com.br"
//   TIM:    "timbrasil.br"
//   Oi:     "gprs.oi.com.br"
// If unsure, try "" (empty) — some carriers auto-configure
#define CELLULAR_APN      ""

// GPS (integrated in A7670E via AT commands)
// No extra pins needed — GPS data comes through the same UART

// ============================================================================
// MAX17048 Fuel Gauge (on-board, on its OWN I2C bus — NOT shared with BNO086)
//
// IMPORTANT: The Waveshare ESP32-S3-A7670E-4G has two board versions
// with DIFFERENT I2C pins for the MAX17048:
//
//   V1:  SDA = GPIO 3,  SCL = GPIO 2   → Wire.begin(3, 2)
//   V2:  SDA = GPIO 15, SCL = GPIO 16  → Wire.begin(15, 16)
//
// On V2 boards, GPIO 15/16 are shared with the camera SCCB bus.
// If the CAM DIP switch is ON, the camera will conflict with the fuel gauge.
// → Turn the CAM DIP switch OFF when using the MAX17048 on V2.
//
// To determine your version: run an I2C scan on both pin pairs.
// Whichever finds address 0x36 is your MAX17048 bus.
// ============================================================================
#define MAX17048_I2C_ADDR     0x36   // Fixed I2C address

// ---- SELECT YOUR BOARD VERSION HERE ----
// Uncomment ONE of the following pairs:

// V1 board:
// #define MAX17048_SDA          3
// #define MAX17048_SCL          2

// V2 board (turn CAM DIP switch OFF!):
#define MAX17048_SDA          15
#define MAX17048_SCL          16

// MAX17048 register map
#define MAX17048_REG_VCELL    0x02   // Cell voltage (78.125 µV/bit)
#define MAX17048_REG_SOC      0x04   // State of charge (1%/256 per bit)
#define MAX17048_REG_MODE     0x06   // Quick-start, sleep control
#define MAX17048_REG_VERSION  0x08   // IC version
#define MAX17048_REG_CONFIG   0x0C   // RCOMP, sleep, alert threshold
#define MAX17048_REG_CRATE    0x16   // Charge/discharge rate (%/hr, signed)
#define MAX17048_REG_VRESET   0x18   // Voltage reset threshold
#define MAX17048_REG_STATUS   0x1A   // Alert status flags
#define MAX17048_REG_COMMAND  0xFE   // POR command

// Battery thresholds
#define MAX17048_ALERT_THRESHOLD  5  // Low-SoC alert at 10% (1–32%)
#define MAX17048_CRITICAL_PCT     5  // Below this: extended sleep mode

// Put the MAX17048 to sleep alongside the ESP32 to save ~22.5 µA.
// Set to false if you want continuous SoC tracking during deep sleep.
#define MAX17048_SLEEP_WITH_ESP   false

// ============================================================================
// TIMING & THRESHOLDS
// ============================================================================

// Deep-sleep periodic wake interval (seconds)
#define SLEEP_INTERVAL_SEC        300   // 5 minutes — GPS heartbeat / check-in

// Fall detection thresholds (tuned for BNO086 linear accel in m/s²)
#define FALL_ACCEL_THRESHOLD      30.0  // Sudden spike (free-fall → impact)
#define FALL_GYRO_THRESHOLD       250.0 // Rapid rotation (degrees/sec)
#define FALL_FREEFALL_THRESHOLD   2.0   // Near-zero accel during free-fall
#define FALL_FREEFALL_DURATION_MS 80    // Min free-fall window

// Post-fall stationary confirmation (seconds)
#define FALL_CONFIRM_WINDOW_SEC   5

// Alert / communication
#define ALERT_PHONE_NUMBER  "+5511979614028"   // Emergency contact SMS number
#define ALERT_SERVER_URL    "http://iot.gtpc.com.br/api/alert"

// WiFi credentials (used when available — faster + lower power than 4G)
#define WIFI_SSID           "CLARO_2G13A70A"
#define WIFI_PASSWORD       "B813A70A"

// ============================================================================
// MQTT — Mosquitto broker on your VPS
// ============================================================================
#define MQTT_BROKER         "iot.gtpc.com.br"
#define MQTT_PORT           1883        // Use 8883 for TLS
#define MQTT_USER           "elderguard"
#define MQTT_PASSWORD       "@@elderGuard##"  // Set to match your Mosquitto passwd file
#define MQTT_CLIENT_ID      "elderguard-001"

// Topic hierarchy: elderguard/{device_id}/{channel}
// Publishes:
//   elderguard/001/alert     — panic, fall events (QoS 1, retained)
//   elderguard/001/location  — GPS fixes (QoS 1, retained)
//   elderguard/001/telemetry — heartbeat: battery, signal, boot count (QoS 1, retained)
//   elderguard/001/status    — online/offline via LWT (QoS 1, retained)
//   elderguard/001/debug     — serial-style boot log (QoS 0, retained)
// Subscribes:
//   elderguard/001/command   — remote commands from dashboard (QoS 1)
#define MQTT_TOPIC_BASE     "elderguard/001"
#define MQTT_TOPIC_ALERT    MQTT_TOPIC_BASE "/alert"
#define MQTT_TOPIC_LOCATION MQTT_TOPIC_BASE "/location"
#define MQTT_TOPIC_TELEM    MQTT_TOPIC_BASE "/telemetry"
#define MQTT_TOPIC_STATUS   MQTT_TOPIC_BASE "/status"
#define MQTT_TOPIC_CMD      MQTT_TOPIC_BASE "/command"
#define MQTT_TOPIC_DEBUG    MQTT_TOPIC_BASE "/debug"

// MQTT over 4G: use the A7670E's TCP stack via AT commands (no WiFi needed)
// MQTT over WiFi: use PubSubClient library when WiFi is available
#define MQTT_KEEPALIVE_SEC  60
#define MQTT_CONNECT_TIMEOUT_MS  10000
#define MQTT_USE_TLS        false       // Set true + port 8883 if you configure TLS

// BLE
#define BLE_DEVICE_NAME     "ElderGuard-001"

// ============================================================================
// SD Card (SDMMC 1-bit mode, as per Waveshare board)
// ============================================================================
#define SD_CLK            5
#define SD_CMD            4
#define SD_DATA           6
#define SD_LOG_FILENAME   "/elderguard_log.csv"

// ============================================================================
// WAKE-UP REASON BITMASK
// ============================================================================
typedef enum {
    WAKE_UNKNOWN     = 0,
    WAKE_TIMER       = 1,
    WAKE_PANIC       = 2,
    WAKE_FALL_IMU    = 3,
    WAKE_EXT1        = 4,
} wake_reason_t;

#endif // CONFIG_H
