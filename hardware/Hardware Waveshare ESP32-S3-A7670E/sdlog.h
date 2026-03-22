#ifndef SDLOG_H
#define SDLOG_H

#include <Arduino.h>
#include <SD_MMC.h>
#include "config.h"

// ============================================================================
// SD Card Logger — SDMMC 1-bit mode, CSV format with timestamps
// ============================================================================

class SDLogger {
public:
    bool ready = false;

    bool begin() {
        if (ready) return true;  // Already mounted

        SD_MMC.setPins(SD_CLK, SD_CMD, SD_DATA);

        if (!SD_MMC.begin("/sdcard", true /* 1-bit mode */)) {
            Serial.println("[SD] Mount failed — check card");
            return false;
        }

        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.printf("[SD] Card mounted: %llu MB\n", cardSize);

        if (!SD_MMC.exists(SD_LOG_FILENAME)) {
            writeHeader();
        }

        ready = true;
        return true;
    }

    void logEvent(const char* timestamp, const char* event,
                  int battPct, float battV, float chargeRate,
                  double lat, double lon, float alt, float speedKmh,
                  uint8_t sats, int signal, int boot, const char* transport) {
        if (!ready) return;

        File f = SD_MMC.open(SD_LOG_FILENAME, FILE_APPEND);
        if (!f) { Serial.println("[SD] Open failed"); return; }

        f.printf("%s,%s,%d,%.2f,%.1f,%.6f,%.6f,%.1f,%.1f,%d,%d,%d,%s\n",
                 timestamp, event, battPct, battV, chargeRate,
                 lat, lon, alt, speedKmh, sats, signal, boot, transport);
        f.close();
        Serial.printf("[SD] Logged: %s %s\n", timestamp, event);
    }

    void logSimple(const char* timestamp, const char* event, const char* detail) {
        if (!ready) return;

        File f = SD_MMC.open(SD_LOG_FILENAME, FILE_APPEND);
        if (!f) return;
        f.printf("%s,%s,,,,,,,,,,,%s\n", timestamp, event, detail);
        f.close();
    }

    void end() {
        if (ready) {
            SD_MMC.end();
            ready = false;
            Serial.println("[SD] Unmounted");
        }
    }

private:
    void writeHeader() {
        File f = SD_MMC.open(SD_LOG_FILENAME, FILE_WRITE);
        if (!f) return;
        f.println("timestamp,event,batt_pct,batt_v,charge_rate,lat,lon,alt,speed_kmh,sats,signal,boot,transport");
        f.close();
        Serial.println("[SD] Created log with header");
    }
};

#endif // SDLOG_H
