// ============================================================================
// ElderGuard ESP-NOW Serial Receiver — for ESP8266 NodeMCU
//
// Flash this to your NodeMCU. Open Serial Monitor at 115200.
// It will print every debug line broadcast by the ESP32-S3 wearable.
//
// No configuration needed — it listens for ESP-NOW broadcasts.
// Keep the NodeMCU within ~100m of the wearable (ESP-NOW range).
// ============================================================================

#include <ESP8266WiFi.h>
#include <espnow.h>

void onReceive(uint8_t *mac, uint8_t *data, uint8_t len) {
    // Print sender MAC (first time only for identification)
    static bool firstMsg = true;
    if (firstMsg) {
        Serial.printf("\n[RX] First message from %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        firstMsg = false;
    }

    // Print the debug line
    char buf[251];
    int copyLen = (len < 250) ? len : 250;
    memcpy(buf, data, copyLen);
    buf[copyLen] = '\0';
    Serial.println(buf);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n========================================");
    Serial.println("ElderGuard ESP-NOW Serial Mirror Receiver");
    Serial.println("========================================");
    Serial.printf("MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("Waiting for ESP-NOW broadcasts...\n");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != 0) {
        Serial.println("ESP-NOW init failed!");
        return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onReceive);
}

void loop() {
    // Nothing to do — onReceive callback handles everything
    delay(100);
}
