#ifndef ESPNOW_MIRROR_H
#define ESPNOW_MIRROR_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// ESP-NOW Serial Mirror — espelha TODA saída 'out' via ESP-NOW broadcast
//
// Comportamento:
//   • Todos os módulos usam a global 'out' (DualPrint) em vez de Serial.
//   • Cada linha completa (\n) é enviada como pacote ESP-NOW broadcast.
//   • O receptor (NodeMCU / outro ESP32) imprime via Serial para debug remoto.
//
// Atenção — conflito WiFi / ESP-NOW:
//   ESP-NOW e WiFi STA (para MQTT) compartilham o mesmo rádio 802.11.
//   A sequência correta é:
//     1. espnowInit()   — WiFi STA + esp_now_init() (sem conectar ao AP)
//     2. espnowDeinit() — esp_now_deinit()  ← ANTES de WiFi.begin()
//     3. mqttMgr.begin() — conecta ao AP normalmente
//     4. mqttMgr.disconnect() + espnowInit() — restaura o mirror após MQTT
//   Esta ordem já é respeitada no sketch principal.
//
// Se DISABLE_ESPNOW estiver definido, 'out' é um alias para Serial e as
// funções espnowInit/Deinit são no-ops.
// ============================================================================

#ifdef DISABLE_ESPNOW

inline void espnowInit()           {}
inline void espnowSend(const char*) {}
inline void espnowDeinit()          {}
#define out Serial

#else  // ESP-NOW ativo

#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

static uint8_t  espnowBroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static bool     espnowReady       = false;

#define ESPNOW_MAX_LEN 240

inline void espnowSend(const char* line) {
    if (!espnowReady) return;
    size_t len = strlen(line);
    if (len == 0) return;
    if (len > ESPNOW_MAX_LEN) len = ESPNOW_MAX_LEN;
    esp_now_send(espnowBroadcast, (const uint8_t*)line, len);
    delay(2);  // Dá tempo ao driver de enfileirar o pacote
}

// DualPrint: acumula caracteres e envia ao ESP-NOW a cada '\n' ou quando
// o buffer enche. A saída Serial é imediata (byte a byte).
class DualPrint : public Print {
public:
    size_t write(uint8_t c) override {
        Serial.write(c);
        if (bufPos < sizeof(buf) - 1) buf[bufPos++] = (char)c;
        if (c == '\n' || bufPos >= ESPNOW_MAX_LEN) flushESPNOW();
        return 1;
    }

    size_t write(const uint8_t* data, size_t len) override {
        for (size_t i = 0; i < len; i++) write(data[i]);
        return len;
    }

    void flushESPNOW() {
        if (bufPos > 0) {
            buf[bufPos] = '\0';
            espnowSend(buf);
            bufPos = 0;
        }
    }

    // Alias para compatibilidade com código que chama out.flush()
    void flush() { flushESPNOW(); Serial.flush(); }

private:
    char   buf[ESPNOW_MAX_LEN + 1];
    size_t bufPos = 0;
};

// Instância global — usar 'out' em todos os módulos
static DualPrint out;

inline void espnowInit() {
    // Inicializa WiFi STA sem conectar ao AP — necessário para ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);

#if defined(ESPNOW_CHANNEL) && ESPNOW_CHANNEL > 0 && ESPNOW_CHANNEL <= 13
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
#endif

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW] Init falhou");
        return;
    }

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, espnowBroadcast, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESPNOW] Add peer falhou");
        esp_now_deinit();
        return;
    }

    espnowReady = true;
    out.printf("[ESPNOW] Mirror pronto (ch=%d)\n", ESPNOW_CHANNEL);
}

inline void espnowDeinit() {
    if (espnowReady) {
        out.flushESPNOW();
        esp_now_deinit();
        espnowReady = false;
    }
}

#endif  // DISABLE_ESPNOW
#endif  // ESPNOW_MIRROR_H
