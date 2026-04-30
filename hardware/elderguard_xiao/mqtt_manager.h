#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

// ============================================================================
// MQTT Manager — XIAO ESP32S3
// Único transporte disponível: WiFi + PubSubClient
// (modem 4G e GPS removidos)
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "espnow_mirror.h"

typedef void (*MqttCommandCallback)(const char* topic, const char* payload);

class MqttManager {
public:
    enum Transport { NONE, WIFI_MQTT };

    MqttManager() : wifiPubsub(wifiClient) {}

    bool begin() {
        transport = NONE;

        if (!connectWiFi()) {
            out.println("[MQTT] WiFi falhou — sem conexão disponível");
            return false;
        }

        wifiPubsub.setServer(MQTT_BROKER, MQTT_PORT);
        wifiPubsub.setKeepAlive(MQTT_KEEPALIVE_SEC);
        wifiPubsub.setCallback([this](char* topic, byte* payload, unsigned int len) {
            char buf[256];
            unsigned int n = min(len, (unsigned int)(sizeof(buf) - 1));
            memcpy(buf, payload, n);
            buf[n] = '\0';
            if (cmdCallback) cmdCallback(topic, buf);
        });

        if (connectMqttVia(wifiPubsub, "WiFi")) {
            activePubsub = &wifiPubsub;
            transport = WIFI_MQTT;
            return true;
        }

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    bool publish(const char* topic, const char* payload, bool retained = false) {
        if (!activePubsub || !activePubsub->connected()) {
            out.printf("[MQTT] Desconectado — não pode publicar em %s\n", topic);
            return false;
        }
        bool ok = activePubsub->publish(topic, payload, retained);
        out.printf("[MQTT] %s %s\n", ok ? "→" : "FALHOU", topic);
        return ok;
    }

    // Telemetria sem GPS e sem sinal celular
    bool publishTelemetry(int battPct, float battV, float chargeRate,
                          int bootCount, const char* ts) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"batt_pct\":%d,\"batt_v\":%.2f,\"charge_rate\":%.1f,"
            "\"rssi\":%d,\"boot\":%d,\"ts\":\"%s\"}",
            battPct, battV, chargeRate, WiFi.RSSI(), bootCount, ts);
        return publish(MQTT_TOPIC_TELEM, json, true);
    }

    bool publishAlert(const char* alertType, int battPct,
                      float peakAccel, float peakGyro, const char* ts) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"type\":\"%s\",\"batt\":%d,"
            "\"peak_accel\":%.1f,\"peak_gyro\":%.1f,\"ts\":\"%s\"}",
            alertType, battPct, peakAccel, peakGyro, ts);
        return publish(MQTT_TOPIC_ALERT, json, true);
    }

    bool publishDebug(const char* log) {
        return publish(MQTT_TOPIC_DEBUG, log, true);
    }

    void setCommandCallback(MqttCommandCallback cb) { cmdCallback = cb; }

    bool subscribeCommands() {
        return (activePubsub && activePubsub->connected())
               ? activePubsub->subscribe(MQTT_TOPIC_CMD, 1)
               : false;
    }

    // Subscreve o tópico de acknowledge de alerta (apaga LED no dispositivo)
    // Payload pode ser qualquer coisa — basta publicar no tópico.
    // Exemplo: mosquitto_pub -h broker -t elderguard/001/ack -m "ok"
    bool subscribeAck() {
        return (activePubsub && activePubsub->connected())
               ? activePubsub->subscribe(MQTT_TOPIC_ACK, 1)
               : false;
    }

    void loop() { if (activePubsub) activePubsub->loop(); }

    void disconnect() {
        if (activePubsub && activePubsub->connected()) {
            activePubsub->publish(MQTT_TOPIC_STATUS, "{\"online\":false}", true);
            activePubsub->disconnect();
        }
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        activePubsub = nullptr;
        transport = NONE;
        out.println("[MQTT] Desconectado");
    }

    bool isConnected() { return activePubsub && activePubsub->connected(); }
    Transport getTransport() { return transport; }

private:
    WiFiClient    wifiClient;
    PubSubClient  wifiPubsub;
    PubSubClient* activePubsub = nullptr;
    Transport     transport = NONE;
    MqttCommandCallback cmdCallback = nullptr;

    bool connectWiFi() {
        if (strcmp(WIFI_SSID, "YourSSID") == 0 || strlen(WIFI_SSID) == 0) {
            out.println("[WIFI] SSID não configurado — pulado");
            return false;
        }
        WiFi.persistent(false);
        WiFi.mode(WIFI_OFF);
        delay(100);
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true, true);
        delay(100);

        out.printf("[WIFI] Conectando a %s", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long t0 = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - t0) < 12000) {
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

    bool connectMqttVia(PubSubClient& ps, const char* label) {
        for (int i = 0; i < 3; i++) {
            out.printf("[MQTT] %s tentativa %d/3...\n", label, i + 1);
            if (ps.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                           MQTT_TOPIC_STATUS, 1, true, "{\"online\":false}")) {
                ps.publish(MQTT_TOPIC_STATUS, "{\"online\":true}", true);
                out.printf("[MQTT] Conectado via %s\n", label);
                return true;
            }
            out.printf("[MQTT] rc=%d\n", ps.state());
            delay(2000);
        }
        out.printf("[MQTT] %s falhou após 3 tentativas\n", label);
        return false;
    }
};

#endif // MQTT_MANAGER_H
