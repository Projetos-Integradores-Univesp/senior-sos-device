#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

// ============================================================================
// MQTT Manager — uses TinyGsmClient (from CellularManager) + PubSubClient
//
// Transport priority:
//   1. WiFi + PubSubClient  (if configured + in range)
//   2. 4G + TinyGsmClient + PubSubClient  (cellular TCP, most reliable)
//
// The old AT+CMQTT and hand-crafted TCP approaches are gone.
// TinyGsmClient provides a standard Arduino Client interface over the
// modem's TCP stack, and PubSubClient speaks MQTT over it. Simple.
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Forward-declare CellularManager so we can get its TinyGsmClient
class CellularManager;

typedef void (*MqttCommandCallback)(const char* topic, const char* payload);

class MqttManager {
public:
    enum Transport { NONE, WIFI_MQTT, CELLULAR_MQTT };

    MqttManager() : wifiPubsub(wifiClient) {}

    // -----------------------------------------------------------------------
    // Connect: WiFi first, then 4G via TinyGsmClient
    // -----------------------------------------------------------------------

    bool begin(CellularManager* cell) {
        cellMgr = cell;
        transport = NONE;

        // --- Try WiFi ---
        if (connectWiFi()) {
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
        }

        // --- 4G via TinyGsmClient ---
        if (cellMgr && cellMgr->hasData()) {
            cellPubsub.setClient(cellMgr->getClient());
            cellPubsub.setServer(MQTT_BROKER, MQTT_PORT);
            cellPubsub.setKeepAlive(MQTT_KEEPALIVE_SEC);
            cellPubsub.setCallback([this](char* topic, byte* payload, unsigned int len) {
                char buf[256];
                unsigned int n = min(len, (unsigned int)(sizeof(buf) - 1));
                memcpy(buf, payload, n);
                buf[n] = '\0';
                if (cmdCallback) cmdCallback(topic, buf);
            });

            if (connectMqttVia(cellPubsub, "4G")) {
                activePubsub = &cellPubsub;
                transport = CELLULAR_MQTT;
                return true;
            }
        }

        Serial.println("[MQTT] All transports failed");
        return false;
    }

    // -----------------------------------------------------------------------
    // Publish
    // -----------------------------------------------------------------------

    bool publish(const char* topic, const char* payload, bool retained = false) {
        if (!activePubsub || !activePubsub->connected()) {
            Serial.printf("[MQTT] Not connected, can't publish to %s\n", topic);
            return false;
        }
        bool ok = activePubsub->publish(topic, payload, retained);
        if (ok) Serial.printf("[MQTT] → %s\n", topic);
        else    Serial.printf("[MQTT] Publish failed: %s\n", topic);
        return ok;
    }

    // -----------------------------------------------------------------------
    // Convenience publishers with timestamps
    // -----------------------------------------------------------------------

    bool publishAlert(const char* alertType, double lat, double lon,
                      int battPct, float peakAccel, float peakGyro,
                      const char* ts) {
        char json[384];
        snprintf(json, sizeof(json),
            "{\"type\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,"
            "\"batt\":%d,\"peak_accel\":%.1f,\"peak_gyro\":%.1f,"
            "\"ts\":\"%s\"}",
            alertType, lat, lon, battPct, peakAccel, peakGyro, ts);
        return publish(MQTT_TOPIC_ALERT, json, true);
    }

    bool publishLocation(double lat, double lon, float alt,
                         float speedKmh, uint8_t sats, const char* ts) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f,"
            "\"speed_kmh\":%.1f,\"sats\":%d,\"ts\":\"%s\"}",
            lat, lon, alt, speedKmh, sats, ts);
        return publish(MQTT_TOPIC_LOCATION, json, true);
    }

    bool publishTelemetry(int battPct, float battV, float chargeRate,
                          int signal, int bootCount, const char* ts) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"batt_pct\":%d,\"batt_v\":%.2f,\"charge_rate\":%.1f,"
            "\"signal\":%d,\"boot\":%d,\"transport\":\"%s\",\"ts\":\"%s\"}",
            battPct, battV, chargeRate, signal, bootCount,
            transport == WIFI_MQTT ? "wifi" : "4g", ts);
        return publish(MQTT_TOPIC_TELEM, json, true);
    }

    bool publishDebug(const char* log) {
        return publish(MQTT_TOPIC_DEBUG, log, true);
    }

    // -----------------------------------------------------------------------
    // Subscribe & process
    // -----------------------------------------------------------------------

    void setCommandCallback(MqttCommandCallback cb) { cmdCallback = cb; }

    bool subscribeCommands() {
        if (activePubsub && activePubsub->connected()) {
            return activePubsub->subscribe(MQTT_TOPIC_CMD, 1);
        }
        return false;
    }

    void loop() {
        if (activePubsub) activePubsub->loop();
    }

    // -----------------------------------------------------------------------
    // Disconnect
    // -----------------------------------------------------------------------

    void disconnect() {
        if (activePubsub && activePubsub->connected()) {
            activePubsub->publish(MQTT_TOPIC_STATUS, "{\"online\":false}", true);
            activePubsub->disconnect();
        }
        if (transport == WIFI_MQTT) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
        }
        activePubsub = nullptr;
        transport = NONE;
        Serial.println("[MQTT] Disconnected");
    }

    bool isConnected() {
        return activePubsub && activePubsub->connected();
    }

    Transport getTransport() { return transport; }

private:
    WiFiClient     wifiClient;
    PubSubClient   wifiPubsub;
    PubSubClient   cellPubsub;   // Uses TinyGsmClient set via setClient()
    PubSubClient*  activePubsub = nullptr;
    CellularManager* cellMgr = nullptr;
    Transport      transport = NONE;
    MqttCommandCallback cmdCallback = nullptr;

    // -----------------------------------------------------------------------
    // WiFi
    // -----------------------------------------------------------------------

    bool connectWiFi() {
        if (strcmp(WIFI_SSID, "YourSSID") == 0 || strlen(WIFI_SSID) == 0) {
            Serial.println("[WIFI] Skipped — no SSID configured");
            return false;
        }

        Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
        Serial.flush();

        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < 8000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WIFI] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
            return true;
        }
        Serial.println("[WIFI] Failed");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    // -----------------------------------------------------------------------
    // MQTT connect via any PubSubClient instance
    // -----------------------------------------------------------------------

    bool connectMqttVia(PubSubClient& ps, const char* label) {
        for (int i = 0; i < 3; i++) {
            Serial.printf("[MQTT] %s attempt %d/3...\n", label, i + 1);
            if (ps.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD,
                           MQTT_TOPIC_STATUS, 1, true,
                           "{\"online\":false}")) {
                ps.publish(MQTT_TOPIC_STATUS, "{\"online\":true}", true);
                Serial.printf("[MQTT] Connected via %s\n", label);
                return true;
            }
            Serial.printf("[MQTT] rc=%d\n", ps.state());
            delay(2000);
        }
        Serial.printf("[MQTT] %s failed after 3 attempts\n", label);
        return false;
    }
};

#endif // MQTT_MANAGER_H
