#ifndef ESPNOW_MIRROR_H
#define ESPNOW_MIRROR_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// ============================================================================
// ESP-NOW Serial Mirror — broadcasts debug output to a nearby ESP8266
//
// The ESP32-S3 sends each logDebug() line via ESP-NOW broadcast.
// A companion ESP8266 (NodeMCU) receives and prints to its own Serial.
//
// No pairing needed — uses broadcast address FF:FF:FF:FF:FF:FF.
// WiFi must be in STA mode (which it already is when WiFi MQTT fails).
// ESP-NOW works even without an AP connection.
// ============================================================================

// Broadcast address — all ESP-NOW peers receive this
static uint8_t espnowBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool espnowReady = false;

// Max ESP-NOW payload is 250 bytes
#define ESPNOW_MAX_LEN 240

void espnowInit() {
    // ESP-NOW needs WiFi in STA mode but does NOT need an AP connection
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  // Don't connect to any AP
    delay(50);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init failed");
        return;
    }

    // Register broadcast peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, espnowBroadcast, 6);
    peer.channel = 0;  // Use current channel
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESPNOW] Add peer failed");
        return;
    }

    espnowReady = true;
    Serial.println("[ESPNOW] Mirror ready (broadcast)");
}

// Send a debug line via ESP-NOW (truncates at 240 chars)
void espnowSend(const char* line) {
    if (!espnowReady) return;

    int len = strlen(line);
    if (len > ESPNOW_MAX_LEN) len = ESPNOW_MAX_LEN;

    esp_now_send(espnowBroadcast, (const uint8_t*)line, len);
}

void espnowDeinit() {
    if (espnowReady) {
        esp_now_deinit();
        espnowReady = false;
    }
}

#endif // ESPNOW_MIRROR_H
