#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

// ============================================================================
// MQTT Manager — ElderGuard XIAO ESP32S3
//
// Tópicos construídos em runtime a partir do device ID:
//   devices/{id}/fall            ← queda confirmada
//   devices/{id}/button-pressed  ← botão de pânico
//   devices/{id}/telemetry       ← heartbeat periódico
//   devices/{id}/status          ← online/offline (LWT, retained)
//   devices/{id}/ack             ← subscribe: apagar LED (responsável → disp.)
//
// Credenciais do broker: definidas via variáveis de ambiente SECRET_*
// injetadas no build pelo platformio.ini (ver config.h e platformio.ini).
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "espnow_mirror.h"

typedef void (*MqttCommandCallback)(const char* topic, const char* payload);

class MqttManager {
public:
    MqttManager() : wifiPubsub(wifiClient) {}

    // -------------------------------------------------------------------------
    // setDeviceId() — deve ser chamado antes de begin()
    // Constrói todos os tópicos dinamicamente.
    // -------------------------------------------------------------------------
    void setDeviceId(const char* id) {
        strncpy(deviceId, id, sizeof(deviceId) - 1);
        deviceId[sizeof(deviceId) - 1] = '\0';

        // Montar tópicos: "devices/{id}/{sufixo}"
        snprintf(topicFall,   sizeof(topicFall),   "%s%s%s",
                 MQTT_TOPIC_PREFIX, deviceId, MQTT_TOPIC_FALL);
        snprintf(topicButton, sizeof(topicButton),
                 "%s%s%s", MQTT_TOPIC_PREFIX, deviceId, MQTT_TOPIC_BUTTON);
        snprintf(topicTelem,  sizeof(topicTelem),  "%s%s%s",
                 MQTT_TOPIC_PREFIX, deviceId, MQTT_TOPIC_TELEMETRY);
        snprintf(topicStatus, sizeof(topicStatus), "%s%s%s",
                 MQTT_TOPIC_PREFIX, deviceId, MQTT_TOPIC_STATUS);
        snprintf(topicAck,    sizeof(topicAck),    "%s%s%s",
                 MQTT_TOPIC_PREFIX, deviceId, MQTT_TOPIC_ACK);

        // Client ID único por dispositivo
        snprintf(clientId, sizeof(clientId), "elderguard-%s", deviceId);

        out.printf("[MQTT] Tópicos:\n"
                   "  fall:    %s\n"
                   "  button:  %s\n"
                   "  telem:   %s\n"
                   "  status:  %s\n"
                   "  ack:     %s\n",
                   topicFall, topicButton, topicTelem, topicStatus, topicAck);
    }

    // -------------------------------------------------------------------------
    // begin() — conecta WiFi e MQTT
    // WiFi SSID/senha são passados como parâmetro (vêm do NVS via BLEManager)
    // -------------------------------------------------------------------------
    bool begin(const char* ssid, const char* wifiPass) {
        if (strlen(deviceId) == 0) {
            out.println("[MQTT] ERRO: device ID não definido — chamar setDeviceId() primeiro");
            return false;
        }

        if (!connectWiFi(ssid, wifiPass)) return false;

        wifiPubsub.setServer(MQTT_BROKER, MQTT_PORT);
        wifiPubsub.setKeepAlive(MQTT_KEEPALIVE_SEC);
        wifiPubsub.setCallback([this](char* topic, byte* payload, unsigned int len) {
            char buf[256];
            unsigned int n = min(len, (unsigned int)(sizeof(buf) - 1));
            memcpy(buf, payload, n);
            buf[n] = '\0';
            if (cmdCallback) cmdCallback(topic, buf);
        });

        if (connectMqtt()) {
            activePubsub = &wifiPubsub;
            return true;
        }

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    // -------------------------------------------------------------------------
    // publish() — genérico
    // -------------------------------------------------------------------------
    bool publish(const char* topic, const char* payload, bool retained = false) {
        if (!activePubsub || !activePubsub->connected()) {
            out.printf("[MQTT] Desconectado — não pode publicar em %s\n", topic);
            return false;
        }
        bool ok = activePubsub->publish(topic, payload, retained);
        out.printf("[MQTT] %s %s\n", ok ? "→" : "FALHOU", topic);
        return ok;
    }

    // -------------------------------------------------------------------------
    // publishFall() — "devices/{id}/fall"
    // -------------------------------------------------------------------------
    bool publishFall(int battPct, float peakAccel, float peakGyro,
                     const char* ts) {
        // Payload texto puro para compatibilidade com o backend
        return publish(topicFall, "FALL", false);
    }

    // -------------------------------------------------------------------------
    // publishButtonPressed() — "devices/{id}/button-pressed"
    // -------------------------------------------------------------------------
    bool publishButtonPressed(int battPct, const char* ts) {
        // Payload texto puro para compatibilidade com o backend
        return publish(topicButton, "BUTTON_PRESSED", false);
    }

    // -------------------------------------------------------------------------
    // publishTelemetry() — "devices/{id}/telemetry"
    // -------------------------------------------------------------------------
    bool publishTelemetry(int battPct, float battV, float chargeRate,
                          int bootCount, const char* ts) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"device_id\":\"%s\",\"batt_pct\":%d,\"batt_v\":%.2f,"
            "\"charge_rate\":%.1f,\"rssi\":%d,\"boot\":%d,\"ts\":\"%s\"}",
            deviceId, battPct, battV, chargeRate,
            WiFi.RSSI(), bootCount, ts);
        return publish(topicTelem, json, true);   // retained: último heartbeat
    }

    // -------------------------------------------------------------------------
    // subscribeAck() — "devices/{id}/ack"
    // -------------------------------------------------------------------------
    bool subscribeAck() {
        if (!activePubsub || !activePubsub->connected()) return false;
        return activePubsub->subscribe(topicAck, 1);
    }

    void setCommandCallback(MqttCommandCallback cb) { cmdCallback = cb; }
    void loop() { if (activePubsub) activePubsub->loop(); }
    bool isConnected() { return activePubsub && activePubsub->connected(); }

    // Getters de tópico (usados no callback para identificar mensagem)
    const char* getTopicAck()    const { return topicAck; }
    const char* getTopicFall()   const { return topicFall; }
    const char* getTopicButton() const { return topicButton; }
    const char* getDeviceId()    const { return deviceId; }

    void disconnect() {
        if (activePubsub && activePubsub->connected()) {
            // LWT manual antes de desconectar (broker já tem o LWT configurado,
            // mas publicar garante o retained imediato)
            publish(topicStatus, "{\"online\":false}", true);
            activePubsub->disconnect();
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        activePubsub = nullptr;
        out.println("[MQTT] Desconectado");
    }

private:
    WiFiClient    wifiClient;
    PubSubClient  wifiPubsub;
    PubSubClient* activePubsub = nullptr;
    MqttCommandCallback cmdCallback = nullptr;

    char deviceId   [48]                = {};
    char clientId   [64]                = {};
    char topicFall  [MQTT_TOPIC_MAX_LEN] = {};
    char topicButton[MQTT_TOPIC_MAX_LEN] = {};
    char topicTelem [MQTT_TOPIC_MAX_LEN] = {};
    char topicStatus[MQTT_TOPIC_MAX_LEN] = {};
    char topicAck   [MQTT_TOPIC_MAX_LEN] = {};

    bool connectWiFi(const char* ssid, const char* pass) {
        if (!ssid || strlen(ssid) == 0) {
            out.println("[WIFI] SSID vazio — não conectando");
            return false;
        }
        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true, true);
        delay(100);

        out.printf("[WIFI] Conectando a \"%s\"", ssid);
        WiFi.begin(ssid, pass);

        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 15000) {
            delay(300);
            out.print(".");
        }
        out.println();

        if (WiFi.status() == WL_CONNECTED) {
            out.printf("[WIFI] IP: %s  RSSI: %d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
            return true;
        }
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        out.printf("[WIFI] Falhou (status=%d)\n", WiFi.status());
        return false;
    }

    bool connectMqtt() {
        // LWT: publicar offline se conexão cair inesperadamente
        char lwtPayload[80];
        snprintf(lwtPayload, sizeof(lwtPayload),
                 "{\"device_id\":\"%s\",\"online\":false}", deviceId);

        for (int i = 0; i < 3; i++) {
            out.printf("[MQTT] Tentativa %d/3 → %s:%d\n", i+1, MQTT_BROKER, MQTT_PORT);
            if (wifiPubsub.connect(clientId,
                                   MQTT_USERNAME,
                                   MQTT_PASSWORD_BROKER,
                                   topicStatus, 1, true, lwtPayload)) {
                // Publicar online imediatamente
                char onlinePayload[80];
                snprintf(onlinePayload, sizeof(onlinePayload),
                         "{\"device_id\":\"%s\",\"online\":true}", deviceId);
                wifiPubsub.publish(topicStatus, onlinePayload, true);
                out.printf("[MQTT] Conectado como \"%s\"\n", clientId);
                return true;
            }
            out.printf("[MQTT] rc=%d\n", wifiPubsub.state());
            delay(2000);
        }
        out.println("[MQTT] Falhou após 3 tentativas");
        return false;
    }
};

#endif // MQTT_MANAGER_H
