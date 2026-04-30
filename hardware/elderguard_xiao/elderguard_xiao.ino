// ============================================================================
// ElderGuard — XIAO ESP32S3 (Seeed Studio)
// Hardware: MPU6500 + MAX17048 (I2C único), LiPo 350 mAh
// Comunicação: WiFi / MQTT + ESP-NOW serial mirror
// Sem: modem 4G, GPS, SD card
//
// LED DE ALERTA (ALERT_LED_PIN, ativo LOW):
//   • Acende em qualquer evento de emergência (queda ou pânico)
//   • Permanece aceso durante deep sleep via rtc_gpio_hold_en()
//   • Apaga somente após receber "ack" no tópico MQTT_TOPIC_ACK
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
RTC_DATA_ATTR int  bootCount            = 0;
RTC_DATA_ATTR bool pendingAlert         = false;
RTC_DATA_ATTR char pendingAlertType[16] = "";

// Estado do LED: true = aceso (aguardando ack do responsável)
RTC_DATA_ATTR bool alertLedActive       = false;

// ----------------------------------------------------------------------------
// Objetos globais
// ----------------------------------------------------------------------------
IMUManager  imuMgr;
MqttManager mqttMgr;
BLEManager  bleMgr;

char tsPlaceholder[24] = "unknown";

// ============================================================================
// Timestamp simples (sem RTC externo)
// ============================================================================
void makeTimestamp(char* buf, size_t len) {
    snprintf(buf, len, "boot%d-ms%lu", bootCount, millis());
}

// ============================================================================
// LED de alerta — active LOW, mantido via RTC GPIO hold durante deep sleep
//
// Fluxo de pino:
//   alertLedInit()  → chamado em todo boot para re-aplicar o estado RTC
//   alertLedOn()    → acende + ativa hold (nível LOW persiste no sleep)
//   alertLedOff()   → apaga + ativa hold em HIGH (evita flutuação no sleep)
// ============================================================================
void alertLedInit() {
    // Ao acordar do sleep, o ULP pode estar em execução se o LED estava ativo.
    // Se alertLedActive=true, o ULP já está piscando — não reconfigurar o pino
    // (isso interromperia o pisca). Apenas garantir que o estado lógico está correto.
    if (!alertLedActive) {
        // LED inativo: garantir HIGH (apagado) com hold
        rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 1);
        rtc_gpio_hold_en((gpio_num_t)ALERT_LED_PIN);
        out.println("[LED] Init: apagado");
    } else {
        // LED ativo: ULP já está piscando. Apenas logar o estado.
        out.println("[LED] Init: piscando (aguardando ack)");
    }
}

// Acende o LED em modo pisca via ULP.
// NÃO chama rtc_gpio_hold — o ULP controla o pino diretamente.
// O pisca começa imediatamente e continua durante o deep sleep.
// enterDeepSleep() em power.h chama UlpBlink::start() antes de dormir
// quando alertLedActive == true.
void alertLedOn() {
    // Configuração inicial do pino (UlpBlink::start() vai finalizar)
    rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
    rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
    rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
    rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 0);   // Acender imediatamente
    alertLedActive = true;
    out.println("[LED] ACESO (pisca ULP será armado no sleep)");
}

void alertLedOff() {
    // Parar ULP e apagar LED (UlpBlink::stop() configura hold em HIGH)
    UlpBlink::stop();
    alertLedActive = false;
}

// ============================================================================
// MQTT — callback de comandos e acks
// ============================================================================
void onMqttCommand(const char* topic, const char* payload) {
    out.printf("[CMD] %s → %s\n", topic, payload);

    // Tópico dedicado de ack
    if (strcmp(topic, MQTT_TOPIC_ACK) == 0) {
        alertLedOff();
        mqttMgr.publish(MQTT_TOPIC_STATUS, "{\"ack\":\"ok\",\"led\":\"off\"}", true);
        return;
    }

    // Tópico de comando genérico
    if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
        if (strcmp(payload, "ack") == 0 || strcmp(payload, "acknowledge") == 0) {
            alertLedOff();
            mqttMgr.publish(MQTT_TOPIC_STATUS, "{\"ack\":\"ok\",\"led\":\"off\"}", true);
        } else if (strcmp(payload, "ping") == 0) {
            makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
            mqttMgr.publishTelemetry(
                PowerManager::batteryPercent(),
                PowerManager::readBatteryVoltage(),
                PowerManager::readChargeRate(),
                bootCount, tsPlaceholder);
        }
    }
}

// ============================================================================
// Helpers de conectividade
// ESP-NOW e WiFi AP-connect não coexistem no mesmo rádio.
// Sequência obrigatória: deinit ESP-NOW → WiFi.begin() → MQTT → disconnect → reinit ESP-NOW
// ============================================================================
bool connectMqtt() {
    espnowDeinit();
    bool ok = mqttMgr.begin();
    if (ok) {
        mqttMgr.setCommandCallback(onMqttCommand);
        mqttMgr.subscribeCommands();
        mqttMgr.subscribeAck();
    }
    return ok;
}

void disconnectMqtt() {
    mqttMgr.disconnect();
    espnowInit();
}

// Bombeia o loop MQTT por ms milissegundos (processa callbacks de ack etc.)
void pumpMqtt(unsigned long ms) {
    unsigned long t0 = millis();
    while (millis() - t0 < ms) { mqttMgr.loop(); delay(10); }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    pinMode(PANIC_BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    delay(100);
    bootCount++;

    // ESP-NOW primeiro — todos os out.* subsequentes espelham via rádio
    espnowInit();
    out.printf("\n=== ElderGuard XIAO Boot #%d ===\n", bootCount);

    wake_reason_t reason = PowerManager::getWakeReason();

    // Restaurar estado do LED (RTC hold já manteve o nível; re-aplica configuração)
    alertLedInit();

    // I2C compartilhado (MPU6500 + MAX17048)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    // Fuel gauge
    PowerManager::beginFuelGauge();
    float battV    = PowerManager::readBatteryVoltage();
    int   battPct  = PowerManager::batteryPercent();
    float battRate = PowerManager::readChargeRate();

    if (battPct >= 0)
        out.printf("[BAT] %.2fV  %d%%  rate:%+.1f%%/hr\n", battV, battPct, battRate);
    else
        out.printf("[BAT] gauge offline, V=%.2f\n", battV);

    if (PowerManager::isAlertActive()) PowerManager::clearAlert();

    if (PowerManager::isBatteryCritical()) {
        out.println("[SYS] Bateria CRÍTICA");
        handleLowBattery(battPct, battV);
        return;
    }

    // Reenviar alerta pendente de ciclo anterior
    if (pendingAlert && strlen(pendingAlertType) > 0) {
        out.printf("[SYS] Reenviando alerta pendente: %s\n", pendingAlertType);
        if (connectMqtt()) {
            makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
            mqttMgr.publishAlert(pendingAlertType, battPct,
                                 imuMgr.getLastPeakAccel(),
                                 imuMgr.getLastPeakGyro(),
                                 tsPlaceholder);
            pumpMqtt(1000);
            pendingAlert = false;
            pendingAlertType[0] = '\0';
            disconnectMqtt();
        }
    }

    pinMode(IMU_INT, INPUT_PULLUP);

    switch (reason) {
        case WAKE_PANIC:    handlePanicButton(); break;
        case WAKE_FALL_IMU: handleFallWake();    break;
        case WAKE_TIMER:    handleTimerWake();   break;
        default:            handleFirstBoot();   break;
    }

    disconnectMqtt();   // Também reinicia ESP-NOW
    PowerManager::enterDeepSleep();
}

void loop() { PowerManager::enterDeepSleep(); }

// ============================================================================
// HANDLERS DE EVENTO
// ============================================================================

void handlePanicButton() {
    out.println("[EVT] BOTÃO DE PÂNICO");
    delay(50);
    if (digitalRead(PANIC_BUTTON_PIN) == HIGH) {
        out.println("[EVT] Ruído — ignorando");
        return;
    }

    alertLedOn();   // Acende LED imediatamente

    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
    int battPct = PowerManager::batteryPercent();

    if (connectMqtt()) {
        mqttMgr.publishAlert("panic", battPct, 0, 0, tsPlaceholder);
        // Aguardar possível ack imediato (responsável online no dashboard)
        pumpMqtt(2000);
    } else {
        pendingAlert = true;
        strncpy(pendingAlertType, "panic", sizeof(pendingAlertType) - 1);
        out.println("[EVT] Alerta salvo — sem WiFi");
    }

    bleMgr.begin();
    delay(500);
    if (bleMgr.deviceConnected)
        bleMgr.notifyStatus(battPct, WiFi.RSSI(), "PANIC");
    delay(2000);
    bleMgr.stop();
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
        alertLedOn();   // Acende LED

        makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));
        int battPct = PowerManager::batteryPercent();

        if (connectMqtt()) {
            mqttMgr.publishAlert("fall", battPct,
                                 imuMgr.getLastPeakAccel(),
                                 imuMgr.getLastPeakGyro(),
                                 tsPlaceholder);
            pumpMqtt(2000);
        } else {
            pendingAlert = true;
            strncpy(pendingAlertType, "fall", sizeof(pendingAlertType) - 1);
            out.println("[EVT] Alerta salvo — sem WiFi");
        }
    } else {
        out.println("[EVT] Sem queda — falso alarme");
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
        mqttMgr.publishTelemetry(battPct, battV, battRate, bootCount, tsPlaceholder);
        // LED aceso → esperar mais tempo pelo ack do responsável
        pumpMqtt(alertLedActive ? 3000 : 800);
    } else {
        out.println("[EVT] WiFi indisponível");
    }

    bleMgr.begin();
    delay(1500);
    if (bleMgr.deviceConnected)
        bleMgr.notifyStatus(battPct, WiFi.RSSI(), alertLedActive ? "ALERT" : "OK");
    bleMgr.stop();
}

void handleFirstBoot() {
    out.println("[EVT] Primeiro boot");

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
    }

    bleMgr.begin();
    out.println("[EVT] BLE ativo por 10 s...");
    delay(10000);
    bleMgr.stop();
}

void handleLowBattery(int battPct, float battV) {
    out.printf("[EVT] Bateria crítica %d%% %.2fV\n", battPct, battV);
    makeTimestamp(tsPlaceholder, sizeof(tsPlaceholder));

    if (connectMqtt()) {
        mqttMgr.publishTelemetry(battPct, battV, 0, bootCount, tsPlaceholder);
        mqttMgr.publishAlert("low_battery", battPct, 0, 0, tsPlaceholder);
        pumpMqtt(500);
        disconnectMqtt();
    } else {
        espnowDeinit();
    }

    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_CRITICAL_SEC * 1000000ULL);
    uint64_t mask = (1ULL << PANIC_BUTTON_PIN);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    rtc_gpio_pullup_en((gpio_num_t)PANIC_BUTTON_PIN);
    // LED mantém estado via hold já configurado por alertLedInit()
    esp_deep_sleep_start();
}
