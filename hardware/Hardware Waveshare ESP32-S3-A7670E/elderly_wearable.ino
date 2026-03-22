// ============================================================================
// ElderGuard — ESP32-S3-A7670E + BNO086 Wearable for Elderly Safety
// ============================================================================

#include "config.h"
#include "power.h"
#include "imu.h"
#include "cellular.h"
#include "mqtt_manager.h"
#include "ble_manager.h"
#include "sdlog.h"
#include "espnow_mirror.h"

RTC_DATA_ATTR int  bootCount = 0;

IMUManager       imuMgr;
CellularManager  cellMgr(Serial1);
MqttManager      mqttMgr;
BLEManager       bleMgr;
SDLogger         sdLog;

char utcTimestamp[24] = "unknown";

// ============================================================================
// Debug log buffer + ESP-NOW mirror
// ============================================================================
#define DEBUG_LOG_SIZE 1024
char debugLog[DEBUG_LOG_SIZE];
int  debugLogPos = 0;

void logDebug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char line[128];
    vsnprintf(line, sizeof(line), fmt, args);

    // Serial output
    Serial.println(line);

    // ESP-NOW broadcast to nearby NodeMCU
    espnowSend(line);

    // MQTT debug buffer
    int len = strlen(line);
    if (debugLogPos + len + 2 < DEBUG_LOG_SIZE) {
        memcpy(debugLog + debugLogPos, line, len);
        debugLog[debugLogPos + len] = '\n';
        debugLogPos += len + 1;
        debugLog[debugLogPos] = '\0';
    }
    va_end(args);
}

void flushDebugLog() {
    if (debugLogPos > 0 && mqttMgr.isConnected()) {
        mqttMgr.publishDebug(debugLog);
    }
    debugLogPos = 0;
    debugLog[0] = '\0';
}

// ============================================================================
// Get real UTC timestamp from cellular network
// ============================================================================
void getTimestamp() {
    if (!cellMgr.getNetworkTime(utcTimestamp, sizeof(utcTimestamp))) {
        // Fallback: use millis-based placeholder
        snprintf(utcTimestamp, sizeof(utcTimestamp), "boot%d-ms%lu",
                 bootCount, millis());
    }
    logDebug("Time: %s", utcTimestamp);
}

// ============================================================================
// MQTT command handler
// ============================================================================
void onMqttCommand(const char* topic, const char* payload) {
    logDebug("CMD: %s", payload);
    String cmd(payload);
    if (cmd == "locate") {
        CellularManager::GPSData gps = acquireGPSFix();
        getTimestamp();
        if (gps.valid)
            mqttMgr.publishLocation(gps.latitude, gps.longitude,
                                     gps.altitude, gps.speed, gps.satellites,
                                     utcTimestamp);
    } else if (cmd == "ping") {
        getTimestamp();
        mqttMgr.publishTelemetry(
            PowerManager::batteryPercent(), PowerManager::readBatteryVoltage(),
            PowerManager::readChargeRate(), cellMgr.getSignalStrength(),
            bootCount, utcTimestamp);
    }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    // CRÍTICO: GPIO 0 é strapping pin do ESP32-S3.
    // Se LOW durante power-on, o chip entra em modo download ("Esperando download").
    // Habilitamos pull-up interno o mais cedo possível para resets por software.
    // Para cold boot (primeira alimentação), é NECESSÁRIO um pull-up EXTERNO:
    //   Resistor 10kΩ entre GPIO 0 e 3.3V
    // Sem o resistor externo, o primeiro boot pode travar se o botão estiver
    // flutuando ou com capacitância no circuito.
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(100);
    bootCount++;
    debugLogPos = 0;
    debugLog[0] = '\0';

    // Init ESP-NOW mirror FIRST so all subsequent logDebug() lines
    // are broadcast to the nearby NodeMCU receiver
    espnowInit();

    logDebug("Início #%d", bootCount);
    wake_reason_t reason = PowerManager::getWakeReason();
    logDebug("Despertar: %d", (int)reason);

    // Init fuel gauge
    PowerManager::beginFuelGauge();
    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();
    if (battPct >= 0)
        logDebug("BAT: %.2fV %d%% rate:%+.1f%%/hr", battV, battPct, battRate);
    else
        logDebug("BAT: medidor offline V=%.2f", battV);

    if (PowerManager::isAlertActive()) {
        PowerManager::clearAlert();
    }
    if (PowerManager::isBatteryCritical()) {
        logDebug("Bateria CRÍTICA");
        handleLowBattery(battPct, battV);
        return;
    }

    // Hardware reset BNO086 to clear any stuck INT from previous session
    pinMode(BNO086_RST, OUTPUT);
    digitalWrite(BNO086_RST, LOW);
    delay(15);
    digitalWrite(BNO086_RST, HIGH);
    delay(300);

    // SD card init is deferred — each handler calls sdLog.begin() when needed
    // This avoids any SPI/I2C bus contention during IMU operations

    switch (reason) {
        case WAKE_PANIC:   handlePanicButton(); break;
        case WAKE_FALL_IMU: handleFallWake();   break;
        case WAKE_TIMER:   handleTimerWake();   break;
        default:           handleFirstBoot();   break;
    }

    mqttMgr.disconnect();
    sdLog.end();
    espnowDeinit();
    PowerManager::enterDeepSleep();
}

void loop() { PowerManager::enterDeepSleep(); }

// ============================================================================
// CONNECTIVITY
// ============================================================================
bool connectMqtt() {
    bool ok = mqttMgr.begin(&cellMgr);
    if (ok) {
        mqttMgr.setCommandCallback(onMqttCommand);
        mqttMgr.subscribeCommands();
        unsigned long start = millis();
        while (millis() - start < 500) { mqttMgr.loop(); delay(10); }
        logDebug("MQTT via %s", mqttMgr.getTransport() == MqttManager::WIFI_MQTT ? "wifi" : "4g");
    } else {
        logDebug("MQTT FALHOU (reg=%d data=%d)", cellMgr.isRegistered(), cellMgr.hasData());
    }
    return ok;
}

void sendAlert(const char* alertType, const CellularManager::GPSData& gps) {
    int battPct = PowerManager::batteryPercent();
    bool mqttSent = false;

    getTimestamp();

    if (connectMqtt()) {
        mqttSent = mqttMgr.publishAlert(
            alertType, gps.latitude, gps.longitude, battPct,
            imuMgr.getLastPeakAccel(), imuMgr.getLastPeakGyro(), utcTimestamp);
        if (gps.valid)
            mqttMgr.publishLocation(gps.latitude, gps.longitude,
                                     gps.altitude, gps.speed, gps.satellites,
                                     utcTimestamp);
        flushDebugLog();
    }

    // SD log
    sdLog.begin();
    sdLog.logSimple(utcTimestamp, alertType, gps.valid ? "with GPS" : "no GPS");

    // SMS fallback for critical alerts
    if (!mqttSent || strcmp(alertType, "panic") == 0 || strcmp(alertType, "fall") == 0) {
        sendSmsFallback(alertType, gps, battPct);
    }
}

void sendSmsFallback(const char* alertType, const CellularManager::GPSData& gps, int battPct) {
    if (!cellMgr.begin()) return;
    char sms[256];
    if (gps.valid)
        snprintf(sms, sizeof(sms),
            "%s ALERT!\nhttps://maps.google.com/?q=%.6f,%.6f\nBat:%d%% Spd:%.0fkm/h\n%s",
            alertType, gps.latitude, gps.longitude, battPct, gps.speed, utcTimestamp);
    else
        snprintf(sms, sizeof(sms), "%s ALERT!\nNo GPS. Bat:%d%%\n%s",
            alertType, battPct, utcTimestamp);
    cellMgr.sendSMS(ALERT_PHONE_NUMBER, sms);
}

// ============================================================================
// WAKE HANDLERS
// ============================================================================

void handlePanicButton() {
    logDebug("BOTÃO DE PÂNICO");
    delay(50);
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
    if (digitalRead(PANIC_BUTTON_PIN) == HIGH) {
        logDebug("Pânico: ruído, ignorando");
        return;
    }
    CellularManager::GPSData gps = acquireGPSFix();
    sendAlert("panic", gps);

    bleMgr.begin();
    delay(500);
    bleMgr.notifyStatus(PowerManager::batteryPercent(), cellMgr.getSignalStrength(), "PANIC");
    if (gps.valid) bleMgr.notifyLocation(gps.latitude, gps.longitude, gps.altitude);
    delay(1000);
    bleMgr.stop();
}

void handleFallWake() {
    logDebug("Despertar IMU — verificando queda");
    if (!imuMgr.begin()) {
        logDebug("Inicialização IMU falhou");
        return;
    }
    imuMgr.enableFallDetectionReports();

    unsigned long deadline = millis() + ((FALL_CONFIRM_WINDOW_SEC + 3) * 1000UL);
    bool fallConfirmed = false;
    while (millis() < deadline) {
        if (imuMgr.detectFall()) { fallConfirmed = true; break; }
        delay(5);
    }

    if (fallConfirmed) {
        logDebug("QUEDA CONFIRMADA");
        CellularManager::GPSData gps = acquireGPSFix();
        sendAlert("fall", gps);
    } else {
        logDebug("Sem queda — falso alarme");
    }
    imuMgr.enableWakeOnMotion();
}

void handleTimerWake() {
    logDebug("Heartbeat periódico");

    if (!cellMgr.begin()) {
        logDebug("Celular FALHOU");
        return;
    }
    logDebug("Modem: reg=%d data=%d", cellMgr.isRegistered(), cellMgr.hasData());

    getTimestamp();

    cellMgr.enableGPS();
    CellularManager::GPSData gps = cellMgr.getGPSLocation(45);
    logDebug("GPS: válido=%d lat=%.4f lon=%.4f alt=%.0f spd=%.1f",
             gps.valid, gps.latitude, gps.longitude, gps.altitude, gps.speed);

    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();
    int   sig      = cellMgr.getSignalStrength();

    if (connectMqtt()) {
        logDebug("MQTT via %s", mqttMgr.getTransport() == MqttManager::WIFI_MQTT ? "wifi" : "4g");
        flushDebugLog();
        mqttMgr.publishTelemetry(battPct, battV, battRate, sig, bootCount, utcTimestamp);
        if (gps.valid)
            mqttMgr.publishLocation(gps.latitude, gps.longitude,
                                     gps.altitude, gps.speed, gps.satellites,
                                     utcTimestamp);
    }

    // SD log — mount, write, will be unmounted in setup() after handler returns
    sdLog.begin();
    sdLog.logEvent(utcTimestamp, "heartbeat", battPct, battV, battRate,
                   gps.latitude, gps.longitude, gps.altitude, gps.speed,
                   gps.satellites, sig, bootCount,
                   mqttMgr.getTransport() == MqttManager::WIFI_MQTT ? "wifi" : "4g");

    bleMgr.begin();
    delay(2000);
    if (bleMgr.deviceConnected) {
        bleMgr.notifyLocation(gps.latitude, gps.longitude, gps.altitude);
        bleMgr.notifyStatus(battPct, sig, "OK");
    }
    bleMgr.stop();
}

void handleFirstBoot() {
    logDebug("Inicialização — primeiro boot");

    if (imuMgr.begin()) {
        imuMgr.enableWakeOnMotion();
        logDebug("IMU armado");
    }
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);

    cellMgr.begin();
    cellMgr.enableGPS();
    getTimestamp();

    sdLog.begin();
    sdLog.logSimple(utcTimestamp, "boot", "first");

    if (connectMqtt()) {
        float battV    = PowerManager::readBatteryVoltage();
        int   battPct  = PowerManager::batteryPercent();
        float battRate = PowerManager::readChargeRate();
        mqttMgr.publishTelemetry(battPct, battV, battRate,
                                  cellMgr.getSignalStrength(), bootCount, utcTimestamp);
        flushDebugLog();
        logDebug("MQTT online via %s",
            mqttMgr.getTransport() == MqttManager::WIFI_MQTT ? "WiFi" : "4G");
    } else {
        char msg[128];
        snprintf(msg, sizeof(msg), "ElderGuard ONLINE (no MQTT)\nBat:%d%%\n%s",
            PowerManager::batteryPercent(), utcTimestamp);
        cellMgr.sendSMS(ALERT_PHONE_NUMBER, msg);
    }

    bleMgr.begin();
    logDebug("BLE 10s...");
    delay(10000);
    bleMgr.stop();
}

// ============================================================================
// HELPERS
// ============================================================================
CellularManager::GPSData acquireGPSFix() {
    if (!cellMgr.begin()) return CellularManager::GPSData{};
    cellMgr.enableGPS();
    return cellMgr.getGPSLocation(45);
}

void handleLowBattery(int battPct, float battV) {
    getTimestamp();
    float battRate = PowerManager::readChargeRate();
    if (connectMqtt()) {
        mqttMgr.publishTelemetry(battPct, battV, battRate,
                                  cellMgr.getSignalStrength(), bootCount, utcTimestamp);
        mqttMgr.publishAlert("low_battery", 0, 0, battPct, 0, 0, utcTimestamp);
        mqttMgr.disconnect();
    } else if (cellMgr.begin()) {
        char msg[64];
        snprintf(msg, sizeof(msg), "LOW BATTERY %d%%! %s", battPct, utcTimestamp);
        cellMgr.sendSMS(ALERT_PHONE_NUMBER, msg);
        cellMgr.powerDown();
    }
    sdLog.begin();
    sdLog.logSimple(utcTimestamp, "low_battery", "extended sleep");
    sdLog.end();

    esp_sleep_enable_timer_wakeup(1800ULL * 1000000ULL);
    uint64_t mask = (1ULL << PANIC_BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    rtc_gpio_pullup_en((gpio_num_t)PANIC_BUTTON_PIN);
    esp_deep_sleep_start();
}
