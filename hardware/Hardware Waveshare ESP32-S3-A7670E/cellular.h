#ifndef CELLULAR_H
#define CELLULAR_H

// ============================================================================
// Gerenciador Celular — usa fork LilyGo do TinyGSM para A7670E
// Funções: init modem, registro rede, GPRS, GPS, SMS, hora da rede
// ============================================================================

#define TINY_GSM_MODEM_SIM7600    // A7670E compatível com AT do SIM7600
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>
#include "config.h"

class CellularManager {
public:
    HardwareSerial& serial;
    TinyGsm        modem;
    TinyGsmClient   tcpClient;

    bool networkRegistered = false;
    bool gprsConnected     = false;

    CellularManager(HardwareSerial& s = Serial1)
        : serial(s), modem(s), tcpClient(modem) {}

    bool isRegistered() { return networkRegistered; }
    bool hasData()      { return gprsConnected; }
    TinyGsmClient& getClient() { return tcpClient; }

    // -----------------------------------------------------------------------
    // Inicialização modem + rede + GPRS
    // -----------------------------------------------------------------------

    bool begin() {
        serial.begin(A7670E_BAUD, SERIAL_8N1, A7670E_RX, A7670E_TX);
        delay(100);

        Serial.println("[4G] Inicializando modem...");
        if (!modem.init()) {
            Serial.println("[4G] init() falhou — acionando pino PWR...");
            pinMode(A7670E_PWR, OUTPUT);
            digitalWrite(A7670E_PWR, HIGH);
            delay(1500);
            digitalWrite(A7670E_PWR, LOW);
            delay(5000);

            if (!modem.init()) {
                Serial.println("[4G] Modem não respondeu");
                return false;
            }
        }

        String name = modem.getModemName();
        Serial.printf("[4G] Modem: %s\n", name.c_str());

        // Aguardar registro na rede
        Serial.println("[4G] Aguardando registro na rede...");
        if (!modem.waitForNetwork(60000L)) {
            Serial.println("[4G] Registro na rede falhou");
            networkRegistered = false;
            return false;
        }
        networkRegistered = true;
        Serial.println("[4G] Registrado na rede");

        // Conectar GPRS / dados LTE
        Serial.printf("[4G] Conectando GPRS com APN: %s\n", CELLULAR_APN);
        if (!modem.gprsConnect(CELLULAR_APN)) {
            Serial.println("[4G] GPRS falhou — tentando APN vazio...");
            if (!modem.gprsConnect("")) {
                Serial.println("[4G] GPRS falhou (dados não disponíveis, SMS ok)");
                gprsConnected = false;
                return true;
            }
        }
        gprsConnected = true;
        Serial.printf("[4G] GPRS conectado, IP: %s\n", modem.getLocalIP().c_str());
        return true;
    }

    void powerDown() {
        modem.gprsDisconnect();
        modem.poweroff();
        gprsConnected = false;
        networkRegistered = false;
        Serial.println("[4G] Modem desligado");
    }

    // -----------------------------------------------------------------------
    // SMS
    // -----------------------------------------------------------------------

    bool sendSMS(const char* number, const char* message) {
        bool ok = modem.sendSMS(String(number), String(message));
        Serial.printf("[SMS] %s\n", ok ? "Enviado" : "Falhou");
        return ok;
    }

    // -----------------------------------------------------------------------
    // GPS — ativa manualmente via AT+CGNSSPWR=1 e aguarda +CGNSSPWR: READY!
    // TinyGSM's enableGPS() pode não aguardar o READY corretamente no A7670E
    // -----------------------------------------------------------------------

    struct GPSData {
        bool    valid = false;
        double  latitude  = 0.0;
        double  longitude = 0.0;
        float   altitude  = 0.0;
        float   speed     = 0.0;   // km/h
        uint8_t satellites = 0;
        char    utcTime[16] = {0};
        char    utcDate[12] = {0};
    };

    bool enableGPS() {
        // Ativar GNSS manualmente — o A7670E precisa de AT+CGNSSPWR=1
        // e depois aguardar a resposta assíncrona +CGNSSPWR: READY!
        modem.sendAT("+CGNSSPWR=1");
        if (modem.waitResponse(10000L, "+CGNSSPWR: READY!") != 1) {
            // Pode já estar ligado — verificar status
            modem.sendAT("+CGNSSPWR?");
            String resp = "";
            if (modem.waitResponse(2000L, resp) == 1) {
                if (resp.indexOf("+CGNSSPWR: 1") >= 0) {
                    Serial.println("[GPS] GNSS já estava ativo");
                    return true;
                }
            }
            Serial.println("[GPS] GNSS não ficou pronto — timeout");
            return false;
        }
        Serial.println("[GPS] GNSS ativado e pronto");
        return true;
    }

    void disableGPS() {
        modem.sendAT("+CGNSSPWR=0");
        modem.waitResponse(2000L);
        Serial.println("[GPS] GNSS desativado");
    }

    GPSData getGPSLocation(int timeout_sec = 90) {
        GPSData gps;

        // IMPORTANTE: O A7670E tem DOIS comandos GPS com formatos DIFERENTES:
        //
        // AT+CGPSINFO  → retorna NMEA ddmm.mmmmmm (conforme manual seção 5.2)
        //   +CGPSINFO:lat,N/S,lon,E/W,date,time,alt,speed(knots),course
        //   Exemplo: 2339.94364,S,04636.46762,W,220326,201829.00,785.1,0.253,
        //   Índices: [0]=lat [1]=N/S [2]=lon [3]=E/W [4]=date [5]=time [6]=alt [7]=spd [8]=course
        //
        // AT+CGNSSINFO → retorna graus decimais JÁ CONVERTIDOS (NÃO é NMEA!)
        //   Exemplo: 3,13,,02,00,23.6657352,S,46.6078262,W,...
        //   Apesar do manual dizer ddmm, o firmware retorna decimal.
        //
        // Usamos AT+CGPSINFO pois o formato NMEA é consistente e nosso parser funciona.

        Serial.printf("[GPS] Aguardando fix (timeout %ds)...\n", timeout_sec);
        unsigned long start = millis();

        while ((millis() - start) < (timeout_sec * 1000UL)) {
            modem.sendAT("+CGPSINFO");
            String resp = "";
            if (modem.waitResponse(2000L, resp) == 1) {
                int idx = resp.indexOf("+CGPSINFO:");
                if (idx >= 0) {
                    String data = resp.substring(idx + 10);
                    int endLine = data.indexOf('\n');
                    if (endLine > 0) data = data.substring(0, endLine);
                    data.trim();

                    // Sem fix = campos vazios ",,,,,,,,"
                    if (data.length() > 10 && !data.startsWith(",")) {
                        if (parseCGPSINFO(data, gps)) {
                            Serial.printf("[GPS] Fix: lat=%.6f lon=%.6f alt=%.1f vel=%.1f\n",
                                          gps.latitude, gps.longitude, gps.altitude, gps.speed);
                            return gps;
                        }
                    }
                }
            }
            delay(3000);
        }

        Serial.println("[GPS] Timeout — sem fix");
        return gps;
    }

private:
    // Parser para AT+CGPSINFO — formato NMEA ddmm.mmmmmm
    // +CGPSINFO:lat,N/S,lon,E/W,date,time,alt,speed(knots),course
    //           [0] [1]  [2] [3] [4]  [5]  [6] [7]          [8]
    bool parseCGPSINFO(const String& data, GPSData& gps) {
        String tok[10];
        int count = 0, pos = 0;
        while (pos < (int)data.length() && count < 10) {
            int comma = data.indexOf(',', pos);
            if (comma < 0) comma = data.length();
            tok[count++] = data.substring(pos, comma);
            pos = comma + 1;
        }

        if (count < 7 || tok[0].length() == 0) return false;

        // Latitude NMEA ddmm.mmmmmm → graus decimais
        gps.latitude = nmeaToDecimal(tok[0]);
        if (tok[1] == "S") gps.latitude = -gps.latitude;

        // Longitude NMEA dddmm.mmmmmm → graus decimais
        gps.longitude = nmeaToDecimal(tok[2]);
        if (tok[3] == "W") gps.longitude = -gps.longitude;

        // Data: DDMMYY
        strncpy(gps.utcDate, tok[4].c_str(), sizeof(gps.utcDate) - 1);

        // Hora: HHMMss.ss → "HH:MM:SS"
        if (tok[5].length() >= 6) {
            snprintf(gps.utcTime, sizeof(gps.utcTime), "%c%c:%c%c:%c%c",
                     tok[5][0], tok[5][1], tok[5][2], tok[5][3],
                     tok[5][4], tok[5][5]);
        }

        // Altitude (metros)
        gps.altitude = tok[6].toFloat();

        // Velocidade (nós → km/h)
        if (count > 7 && tok[7].length() > 0) {
            gps.speed = tok[7].toFloat() * 1.852f;
        }

        gps.valid = true;
        return true;
    }

    // Converte NMEA ddmm.mmmmmm para graus decimais
    static double nmeaToDecimal(const String& nmea) {
        double raw = nmea.toDouble();
        int degrees = (int)(raw / 100.0);
        double minutes = raw - (degrees * 100.0);
        return degrees + (minutes / 60.0);
    }

public:

    // -----------------------------------------------------------------------
    // Hora da rede
    // -----------------------------------------------------------------------

    bool getNetworkTime(char* buf, size_t bufLen) {
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        float tz = 0;

        if (modem.getNetworkTime(&year, &month, &day, &hour, &minute, &second, &tz)) {
            snprintf(buf, bufLen, "%04d-%02d-%02dT%02d:%02d:%02d",
                     year, month, day, hour, minute, second);
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Qualidade do sinal
    // -----------------------------------------------------------------------

    int getSignalStrength() {
        return modem.getSignalQuality();
    }
};

#endif // CELLULAR_H
