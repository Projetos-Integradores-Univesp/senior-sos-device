// ============================================================================
// Diagnóstico GNSS v2 — testa AT+CGPSINFO e AT+CGNSSINFO
// Saída via Serial E ESP-NOW (NodeMCU receptor)
// ============================================================================

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define A7670E_RX   17
#define A7670E_TX   18
#define A7670E_BAUD 115200

static uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool espnowOk = false;

void espnowSetup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(50);
    if (esp_now_init() != ESP_OK) { Serial.println("[ESPNOW] Init falhou"); return; }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcast, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) { Serial.println("[ESPNOW] Peer falhou"); return; }
    espnowOk = true;
}

void out(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[240];
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.println(buf);
    if (espnowOk) {
        esp_now_send(broadcast, (const uint8_t*)buf, strlen(buf));
        delay(8);
    }
    va_end(args);
}

void sendAT(const char* cmd) {
    for (int i = 0; cmd[i]; i++) {
        Serial1.write(cmd[i]);
        delay(10);
    }
    Serial1.write('\r'); delay(10);
    Serial1.write('\n'); delay(10);
}

// Captura resposta completa com timeout
String readResponse(unsigned long timeout = 3000) {
    String resp;
    unsigned long start = millis();
    while (millis() - start < timeout) {
        while (Serial1.available()) {
            char c = (char)Serial1.read();
            resp += c;
        }
        if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
        delay(10);
    }
    return resp;
}

// Tenta parsing NMEA ddmm.mmmm(mm)
void tryParseNMEA(const String& latStr, const String& ns,
                   const String& lonStr, const String& ew) {
    if (latStr.length() == 0 || lonStr.length() == 0) {
        out("  (campos vazios, sem fix)");
        return;
    }

    double latRaw = latStr.toDouble();
    int latDeg = (int)(latRaw / 100.0);
    double latMin = latRaw - (latDeg * 100.0);
    double lat = latDeg + (latMin / 60.0);
    if (ns == "S") lat = -lat;

    double lonRaw = lonStr.toDouble();
    int lonDeg = (int)(lonRaw / 100.0);
    double lonMin = lonRaw - (lonDeg * 100.0);
    double lon = lonDeg + (lonMin / 60.0);
    if (ew == "W") lon = -lon;

    out("  Lat raw: %s %s", latStr.c_str(), ns.c_str());
    out("  Lon raw: %s %s", lonStr.c_str(), ew.c_str());
    out("  Lat dec: %.6f", lat);
    out("  Lon dec: %.6f", lon);
    out("  Esperado: ~-23.665 ~-46.608");
}

int cycle = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(1000);

    espnowSetup();
    out("====== Diag GNSS v2 ======");

    Serial1.begin(A7670E_BAUD, SERIAL_8N1, A7670E_RX, A7670E_TX);
    delay(500);

    // Verificar modem
    out("[1] Modem...");
    sendAT("AT");
    String r = readResponse(2000);
    if (r.indexOf("OK") < 0) { out("  Modem NAO respondeu!"); return; }
    out("  Modem OK");

    // Identificação
    out("[2] Firmware:");
    sendAT("AT+SIMCOMATI");
    r = readResponse(3000);
    // Imprimir linha por linha via espnow (max 240 chars)
    int pos = 0;
    while (pos < (int)r.length()) {
        int nl = r.indexOf('\n', pos);
        if (nl < 0) nl = r.length();
        String line = r.substring(pos, nl);
        line.trim();
        if (line.length() > 0) out("  %s", line.c_str());
        pos = nl + 1;
    }

    // Ligar GNSS
    out("[3] AT+CGNSSPWR=1...");
    sendAT("AT+CGNSSPWR=1");
    r = "";
    unsigned long start = millis();
    while (millis() - start < 15000) {
        while (Serial1.available()) r += (char)Serial1.read();
        if (r.indexOf("READY") >= 0) { out("  +CGNSSPWR: READY!"); break; }
        if (r.indexOf("ERROR") >= 0) { out("  ERRO: %s", r.c_str()); break; }
        delay(100);
    }
    if (r.indexOf("READY") < 0 && r.indexOf("ERROR") < 0) {
        out("  Timeout esperando READY");
        // Verificar se já estava ligado
        sendAT("AT+CGNSSPWR?");
        r = readResponse(2000);
        out("  Status: %s", r.c_str());
    }

    out("[4] Testando ambos comandos a cada 5s...");
    out("========================================");
}

void loop() {
    cycle++;
    out("--- Ciclo %d ---", cycle);

    // Limpar buffer
    while (Serial1.available()) Serial1.read();

    // ========== TESTE 1: AT+CGPSINFO (formato simples, 9 campos) ==========
    out("[CGPSINFO]");
    sendAT("AT+CGPSINFO");
    String resp1 = readResponse(3000);

    int idx1 = resp1.indexOf("+CGPSINFO:");
    if (idx1 >= 0) {
        String data1 = resp1.substring(idx1 + 10);
        int endLine = data1.indexOf('\n');
        if (endLine > 0) data1 = data1.substring(0, endLine);
        data1.trim();
        out("  Raw: +CGPSINFO:%s", data1.c_str());

        // Tokenizar
        String t[12];
        int cnt = 0, p = 0;
        while (p < (int)data1.length() && cnt < 12) {
            int c = data1.indexOf(',', p);
            if (c < 0) c = data1.length();
            t[cnt++] = data1.substring(p, c);
            p = c + 1;
        }
        out("  Campos: %d", cnt);
        for (int i = 0; i < cnt; i++) out("  [%d]=%s", i, t[i].c_str());

        // CGPSINFO: lat,N/S,lon,E/W,date,time,alt,speed,course
        //           [0] [1] [2] [3] [4]  [5]  [6] [7]   [8]
        if (cnt >= 7) tryParseNMEA(t[0], t[1], t[2], t[3]);
    } else {
        out("  Sem resposta CGPSINFO");
        // Imprimir resposta bruta para debug
        resp1.trim();
        if (resp1.length() > 0) out("  Bruto: %s", resp1.c_str());
    }

    delay(1000);
    while (Serial1.available()) Serial1.read();

    // ========== TESTE 2: AT+CGNSSINFO (formato completo, 16 campos) ==========
    out("[CGNSSINFO]");
    sendAT("AT+CGNSSINFO");
    String resp2 = readResponse(3000);

    int idx2 = resp2.indexOf("+CGNSSINFO:");
    if (idx2 >= 0) {
        String data2 = resp2.substring(idx2 + 11);
        int endLine2 = data2.indexOf('\n');
        if (endLine2 > 0) data2 = data2.substring(0, endLine2);
        data2.trim();
        out("  Raw: +CGNSSINFO:%s", data2.c_str());

        // Tokenizar
        String t[20];
        int cnt = 0, p = 0;
        while (p < (int)data2.length() && cnt < 20) {
            int c = data2.indexOf(',', p);
            if (c < 0) c = data2.length();
            t[cnt++] = data2.substring(p, c);
            p = c + 1;
        }
        out("  Campos: %d", cnt);
        for (int i = 0; i < cnt; i++) out("  [%d]=%s", i, t[i].c_str());

        // CGNSSINFO (manual p.17):
        //   mode,GPS_SVs,GLONASS_SVs,BEIDOU_SVs,lat,N/S,lon,E/W,date,time,alt,speed,course,PDOP,HDOP,VDOP
        //   [0]  [1]     [2]         [3]         [4] [5] [6] [7] [8]  [9]  [10][11]  [12]   [13] [14] [15]
        if (cnt >= 11) tryParseNMEA(t[4], t[5], t[6], t[7]);
    } else {
        out("  Sem resposta CGNSSINFO");
        resp2.trim();
        if (resp2.length() > 0) out("  Bruto: %s", resp2.c_str());
    }

    out("");
    delay(5000);
}
