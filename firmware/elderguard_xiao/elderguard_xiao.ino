// ============================================================================
// ElderGuard — XIAO ESP32S3
// Hardware: MPU6500 + MAX17048 (I2C único), LiPo 350 mAh
// Comunicação: WiFi/MQTT (provisionado via BLE) + ESP-NOW mirror
//
// FLUXO DE BOOT:
//   1. Verificar NVS — provisionado?
//      NÃO → modo provisionamento BLE (aguarda app definir SSID/ID/senha)
//      SIM → carregar credenciais do NVS e operar normalmente
//   2. Em modo normal: verificar causa de wake (timer / IMU / botão)
//   3. Publicar no broker MQTT (configurado via SECRET_MQTT_BROKER) com tópicos devices/{id}/...
// ============================================================================

#include "config.h"
#include "espnow_mirror.h"
#include "power.h"
#include "imu.h"
#include "mqtt_manager.h"
#include "ble_manager.h"
#include "ulp_blink.h"

// ----------------------------------------------------------------------------
// RTC RAM — persiste entre deep sleeps
// ----------------------------------------------------------------------------
RTC_DATA_ATTR int  bootCount             = 0;
RTC_DATA_ATTR bool pendingAlert          = false;
RTC_DATA_ATTR char pendingAlertType[16]  = "";
RTC_DATA_ATTR bool alertLedActive        = false;

// ----------------------------------------------------------------------------
// Credenciais carregadas do NVS (heap, não RTC — recarregadas a cada boot)
// ----------------------------------------------------------------------------
static char gSsid    [64]  = {};
static char gPass    [64]  = {};
static char gDeviceId[48]  = {};

// ----------------------------------------------------------------------------
// Objetos globais
// ----------------------------------------------------------------------------
IMUManager  imuMgr;
MqttManager mqttMgr;
BLEManager  bleMgr;

char tsPlaceholder[32] = "";

// ============================================================================
// Helpers
// ============================================================================
void makeTimestamp(char* buf, size_t len) {
    snprintf(buf, len, "boot%d-ms%lu", bootCount, millis());
}

// ============================================================================
// LED de alerta
// ============================================================================
void alertLedInit() {
    if (!alertLedActive) {
        rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 1);
        rtc_gpio_hold_en((gpio_num_t)ALERT_LED_PIN);
    } else {
        out.println("[LED] Init: piscando (aguardando ack)");
    }
}

void alertLedOn() {
    rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
    rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
    rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 0);
    alertLedActive = true;
    out.println("[LED] ACESO (ULP armado no sleep)");
}

void alertLedOff() {
    UlpBlink::stop();
    alertLedActive = false;
}

// ============================================================================
// Callback MQTT — tópico /ack apaga LED
// ============================================================================
void onMqttMessage(const char* topic, const char* payload) {
    out.printf("[MQTT] ← %s: %s\n", topic, payload);
    if (strcmp(topic, mqttMgr.getTopicAck()) == 0) {
        alertLedOff();
        char statusPayload[80];
        snprintf(statusPayload, sizeof(statusPayload),
                 "{\"device_id\":\"%s\",\"ack\":\"ok\",\"led\":\"off\"}",
                 gDeviceId);
        mqttMgr.publish(mqttMgr.getTopicAck(), statusPayload, false);
    }
}

// ============================================================================
// Conectar / desconectar MQTT
// ============================================================================
bool connectMqtt() {
    espnowDeinit();
    mqttMgr.setDeviceId(gDeviceId);
    bool ok = mqttMgr.begin(gSsid, gPass);
    if (ok) {
        mqttMgr.setCommandCallback(onMqttMessage);
        mqttMgr.subscribeAck();
    }
    return ok;
}

void disconnectMqtt() {
    mqttMgr.disconnect();
    espnowInit();
}

void pumpMqtt(unsigned long ms) {
    unsigned long t0 = millis();
    while (millis() - t0 < ms) { mqttMgr.loop(); delay(10); }
}

// ============================================================================
// MODO PROVISIONAMENTO BLE
// Exibido quando o dispositivo nunca foi configurado ou após factory reset.
// O app escreve SSID, senha e ID. O dispositivo grava em NVS e reinicia.
// ============================================================================
void runProvisioningMode() {
    out.println("[SYS] === MODO PROVISIONAMENTO BLE ===");
    out.printf("[SYS] Aguardando app \"%s\"...\n", BLE_ADV_NAME);

    bleMgr.begin();

    unsigned long start = millis();
    bool timedOut = false;

    while (!bleMgr.provisionComplete && !bleMgr.resetRequested) {
        delay(100);

        // Timeout opcional (0 = sem timeout)
        if (BLE_PROVISION_TIMEOUT_MS > 0 &&
            (millis() - start) > BLE_PROVISION_TIMEOUT_MS) {
            timedOut = true;
            break;
        }
    }

    if (timedOut) {
        out.println("[BLE] Timeout sem app — dormindo 60 s e tentando de novo");
        BLEDevice::getAdvertising()->stop();
        esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
        esp_deep_sleep_start();
        return;
    }

    if (bleMgr.resetRequested) {
        BLEDevice::getAdvertising()->stop();
        BLEManager::factoryReset();   // reinicia em modo BLE limpo
        return;
    }

    // Validação mínima
    if (bleMgr.rxSsid.length() == 0 || bleMgr.rxDeviceId.length() == 0) {
        bleMgr.notifyError("ssid_or_id_empty");
        delay(1000);
        BLEDevice::getAdvertising()->stop();
        ESP.restart();
        return;
    }

    // Confirmar para o app antes de reiniciar.
    // NÃO chamar bleMgr.stop() / BLEDevice::deinit() antes do restart:
    // deinit() libera estruturas do BLE com callbacks ainda pendentes,
    // corrompendo o heap. ESP.restart() reseta tudo de forma segura.
    bleMgr.notifyProvisioned(bleMgr.rxDeviceId.c_str());
    delay(1500);   // tempo para o app receber a notificação GATT

    // Parar advertising sem deinit (evita corrupção de heap)
    BLEDevice::getAdvertising()->stop();

    BLEManager::saveAndReboot(
        bleMgr.rxSsid.c_str(),
        bleMgr.rxPass.c_str(),
        bleMgr.rxDeviceId.c_str()
    );
    // saveAndReboot() chama ESP.restart() — não chega aqui
}

// ============================================================================
// HANDLERS DE EVENTO (modo normal)
// ============================================================================

void handlePanicButton() {
    out.println("[EVT] BOTÃO DE PÂNICO");
    delay(50);
    if (digitalRead(PANIC_BUTTON_PIN) == HIGH) {
        out.println("[EVT] Ruído — ignorando");
        return;
    }

    alertLedOn();
    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
    int battPct = PowerManager::batteryPercent();

    if (connectMqtt()) {
        mqttMgr.publishButtonPressed(battPct, tsPlaceholder);
        pumpMqtt(2000);   // aguardar possível ack imediato
        disconnectMqtt();
    } else {
        pendingAlert = true;
        strncpy(pendingAlertType, "button-pressed", sizeof(pendingAlertType) - 1);
        out.println("[EVT] Alerta salvo em RTC RAM — sem WiFi");
    }
}

void handleFallWake() {
    out.println("[EVT] Wake IMU — verificando queda");

    if (!imuMgr.begin()) {
        out.println("[EVT] Falha ao iniciar IMU");
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
        out.println("[EVT] QUEDA CONFIRMADA");
        alertLedOn();
        makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
        int battPct = PowerManager::batteryPercent();

        if (connectMqtt()) {
            mqttMgr.publishFall(battPct,
                                imuMgr.getLastPeakAccel(),
                                imuMgr.getLastPeakGyro(),
                                tsPlaceholder);
            pumpMqtt(2000);
            disconnectMqtt();
        } else {
            pendingAlert = true;
            strncpy(pendingAlertType, "fall", sizeof(pendingAlertType) - 1);
            out.println("[EVT] Alerta salvo em RTC RAM — sem WiFi");
        }
    } else {
        out.println("[EVT] Sem queda confirmada");
    }
    imuMgr.enableWakeOnMotion();
}

void handleTimerWake() {
    out.println("[EVT] Heartbeat periódico");
    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();

    if (connectMqtt()) {
        // Reenviar alerta pendente se houver
        if (pendingAlert && strlen(pendingAlertType) > 0) {
            out.printf("[EVT] Reenviando alerta: %s\n", pendingAlertType);
            if (strcmp(pendingAlertType, "fall") == 0) {
                mqttMgr.publishFall(battPct, 0, 0, tsPlaceholder);
            } else {
                mqttMgr.publishButtonPressed(battPct, tsPlaceholder);
            }
            pendingAlert = false;
            pendingAlertType[0] = '\0';
        }

        mqttMgr.publishTelemetry(battPct, battV, battRate, bootCount, tsPlaceholder);
        pumpMqtt(alertLedActive ? 3000 : 800);
        disconnectMqtt();
    } else {
        out.println("[EVT] WiFi indisponível");
    }
}

void handleFirstBoot() {
    out.println("[EVT] Primeiro boot (modo normal)");

    if (imuMgr.begin()) {
        imuMgr.enableWakeOnMotion();
        out.println("[EVT] IMU armado (WOM)");
    }

    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();

    if (connectMqtt()) {
        mqttMgr.publishTelemetry(battPct, battV, battRate, bootCount, tsPlaceholder);
        pumpMqtt(500);
        disconnectMqtt();
    }
}

void handleLowBattery(int battPct, float battV) {
    out.printf("[EVT] Bateria crítica: %d%% / %.2fV\n", battPct, battV);
    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));

    if (connectMqtt()) {
        mqttMgr.publishTelemetry(battPct, battV, 0, bootCount, tsPlaceholder);
        pumpMqtt(500);
        disconnectMqtt();
    } else {
        espnowDeinit();
    }

    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_CRITICAL_SEC * 1000000ULL);
    uint64_t mask = (1ULL << PANIC_BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    rtc_gpio_pullup_en((gpio_num_t)PANIC_BUTTON_PIN);
    esp_deep_sleep_start();
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    delay(100);
    bootCount++;

    espnowInit();
    out.printf("\n=== ElderGuard XIAO Boot #%d ===\n", bootCount);

    // ---- Verificar se está provisionado ------------------------------------
    if (!BLEManager::isProvisioned()) {
        out.println("[SYS] Não provisionado — iniciando modo BLE");
        runProvisioningMode();
        // runProvisioningMode() nunca retorna em modo normal —
        // sempre reinicia ou dorme.
        return;
    }

    // ---- Carregar credenciais do NVS ----------------------------------------
    if (!BLEManager::loadCredentials(gSsid, sizeof(gSsid),
                                     gPass, sizeof(gPass),
                                     gDeviceId, sizeof(gDeviceId))) {
        out.println("[SYS] ERRO: NVS corrompido — resetando para BLE");
        BLEManager::factoryReset();
        return;
    }
    out.printf("[SYS] Device ID: %s  SSID: %s\n", gDeviceId, gSsid);

    // ---- Restaurar LED ------------------------------------------------------
    alertLedInit();

    // ---- Fuel gauge ---------------------------------------------------------
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    PowerManager::beginFuelGauge();

    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();

    out.printf("[BAT] %.2fV  %d%%  rate:%+.1f%%/hr\n", battV, battPct, battRate);

    if (PowerManager::isAlertActive()) PowerManager::clearAlert();

    if (PowerManager::isBatteryCritical()) {
        handleLowBattery(battPct, battV);
        return;
    }

    // ---- Dispatcher de wake -------------------------------------------------
    wake_reason_t reason = PowerManager::getWakeReason();

    switch (reason) {
        case WAKE_PANIC:    handlePanicButton(); break;
        case WAKE_FALL_IMU: handleFallWake();    break;
        case WAKE_TIMER:    handleTimerWake();   break;
        default:            handleFirstBoot();   break;
    }

    PowerManager::enterDeepSleep();
}

void loop() { PowerManager::enterDeepSleep(); }
