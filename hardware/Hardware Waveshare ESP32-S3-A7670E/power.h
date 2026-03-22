#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"

// ============================================================================
// Power Management — Deep Sleep & MAX17048 Fuel Gauge Battery Monitoring
//
// The MAX17048 is on-board the ESP32-S3-A7670E module. It shares the
// same I2C bus as the BNO086 (SDA=GPIO2, SCL=GPIO1) at address 0x36.
// It provides accurate State-of-Charge (SoC) via Maxim's ModelGauge
// algorithm — no voltage-to-percentage lookup table needed.
//
// Key MAX17048 registers:
//   0x02  VCELL  — cell voltage (12-bit, 78.125 µV/bit)
//   0x04  SOC    — state of charge (1%/256 per bit)
//   0x06  MODE   — quick start, sleep
//   0x08  VERSION
//   0x0C  CONFIG — sleep, alert threshold, RCOMP
//   0x14  CRATE  — charge/discharge rate (%/hr)
//   0x16  VRESET — voltage reset threshold
//   0xFE  COMMAND — POR, quick start
// ============================================================================

class PowerManager {
public:

    // -----------------------------------------------------------------------
    // MAX17048 initialisation — call once after I2C bus is up
    // -----------------------------------------------------------------------

    static bool beginFuelGauge() {
        // MAX17048 is on its OWN I2C bus (separate from BNO086 on Wire)
        Wire1.begin(MAX17048_SDA, MAX17048_SCL);
        delay(50);  // Let I2C pull-ups and bus stabilise after deep sleep
        Wire1.setClock(400000);

        // Verify the chip responds (retry a few times — the gauge may be
        // waking from its own sleep mode and needs a moment)
        uint16_t version = 0;
        for (int attempt = 0; attempt < 5; attempt++) {
            version = readRegister16(MAX17048_REG_VERSION);
            if (version != 0 && version != 0xFFFF) break;
            delay(100);  // Give the gauge time to wake
        }

        if (version == 0 || version == 0xFFFF) {
            Serial.println("[BAT] MAX17048 not found on I2C bus");
            Serial.printf("[BAT]   SDA=GPIO%d, SCL=GPIO%d — check board version (V1/V2)\n",
                          MAX17048_SDA, MAX17048_SCL);
            return false;
        }
        Serial.printf("[BAT] MAX17048 found, version: 0x%04X\n", version);

        // Quick-start: force the gauge to re-evaluate SoC from current voltage.
        quickStart();

        // CRITICAL: Wait for the quick-start to settle. The MAX17048 needs
        // ~500ms after quick-start before the SOC register holds a valid
        // value. Without this delay, SOC reads as 0% and triggers false
        // low-battery alerts.
        delay(600);

        // Set the low-battery alert threshold
        setAlertThreshold(MAX17048_ALERT_THRESHOLD);

        // Clear any pending alert from before sleep
        clearAlert();

        gaugeReady = true;
        return true;
    }

    // -----------------------------------------------------------------------
    // Battery readings
    // -----------------------------------------------------------------------

    // Cell voltage in volts (e.g. 3.82)
    static float readBatteryVoltage() {
        if (!gaugeReady && !beginFuelGauge()) return 0.0f;

        uint16_t raw = readRegister16(MAX17048_REG_VCELL);
        // VCELL is a 16-bit value; voltage = raw × 78.125 µV
        float voltage = raw * 78.125e-6f;
        return voltage;
    }

    // State of Charge in percent (e.g. 73.5)
    // The MAX17048 uses ModelGauge — much more accurate than voltage mapping
    static float readBatterySOC() {
        if (!gaugeReady && !beginFuelGauge()) return -1.0f;  // -1 = gauge offline

        uint16_t raw = readRegister16(MAX17048_REG_SOC);
        if (raw == 0xFFFF) return -1.0f;  // I2C read failed

        // SOC register: high byte = integer %, low byte = 1/256 fractional %
        float soc = (raw >> 8) + (raw & 0xFF) / 256.0f;
        // Clamp to 0–100
        if (soc > 100.0f) soc = 100.0f;
        if (soc < 0.0f)   soc = 0.0f;
        return soc;
    }

    // Integer percent for simple comparisons
    // Returns -1 if the gauge is not responding (treat as unknown, not zero)
    static int batteryPercent() {
        float soc = readBatterySOC();
        if (soc < 0) return -1;  // Gauge offline — don't assume 0%
        return (int)(soc + 0.5f);
    }

    // Charge/discharge rate in %/hr (positive = charging, negative = discharging)
    static float readChargeRate() {
        if (!gaugeReady) return 0.0f;

        uint16_t raw = readRegister16(MAX17048_REG_CRATE);
        if (raw == 0xFFFF) return 0.0f;

        int16_t signed_raw = (int16_t)raw;
        // CRATE: signed 16-bit, 0.208 %/hr per bit
        float rate = signed_raw * 0.208f;
        return rate;
    }

    // Returns true if the gauge's alert flag is set (SoC dropped below threshold)
    static bool isAlertActive() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        if (config == 0xFFFF) return false;  // I2C error — don't false-alarm
        return (config & 0x0020);  // Bit 5 of low byte = ALRT
    }

    // Return true if battery is TRULY critically low.
    // Cross-checks SOC with voltage to avoid false triggers from a stale gauge.
    static bool isBatteryCritical() {
        int pct = batteryPercent();

        // If gauge is offline (-1), DON'T treat as critical — we simply don't know
        if (pct < 0) {
            Serial.println("[BAT] Gauge offline — skipping critical check");
            return false;
        }

        // If SoC says low, cross-check with cell voltage as a sanity gate.
        // A real empty 18650 is below ~3.3V. If voltage reads above 3.5V
        // but SoC says 0%, the gauge hasn't settled — it's a false reading.
        if (pct < MAX17048_CRITICAL_PCT) {
            float v = readBatteryVoltage();
            if (v > 3.5f) {
                Serial.printf("[BAT] SoC=%d%% but voltage=%.2fV — false low, ignoring\n", pct, v);
                return false;
            }
            return true;  // Both SoC and voltage agree: genuinely low
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Fuel gauge power modes
    // -----------------------------------------------------------------------

    // Put the MAX17048 into its own sleep mode to reduce its quiescent current
    // from ~23 µA to ~0.5 µA. The gauge stops tracking SoC while asleep.
    // Call quickStart() after waking to re-sync.
    static void sleepFuelGauge() {
        // To enter sleep: set ENSLEEP bit in CONFIG, then write 0x0000 to MODE
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config |= 0x0080;  // Set ENSLEEP (bit 7 of low byte)
        writeRegister16(MAX17048_REG_CONFIG, config);

        writeRegister16(MAX17048_REG_MODE, 0x0000);
        Serial.println("[BAT] MAX17048 entering sleep (~0.5µA)");
    }

    // Wake the fuel gauge from sleep and quick-start to re-sync SoC
    static void wakeFuelGauge() {
        // Any I2C transaction wakes the MAX17048 from sleep.
        // Then clear ENSLEEP and quick-start.
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config &= ~0x0080;  // Clear ENSLEEP
        writeRegister16(MAX17048_REG_CONFIG, config);

        quickStart();
        Serial.println("[BAT] MAX17048 awake, quick-start issued");
    }

    // Quick-start: forces the gauge to re-calculate SoC from current voltage
    static void quickStart() {
        writeRegister16(MAX17048_REG_MODE, 0x4000);
        delay(2);  // Gauge needs ~1ms to process
    }

    // -----------------------------------------------------------------------
    // Alert configuration
    // -----------------------------------------------------------------------

    // Set the low-SoC alert threshold (1–32%, default 4%)
    // When SoC drops below this, the ALRT flag is set in CONFIG
    static void setAlertThreshold(uint8_t percent) {
        if (percent > 32) percent = 32;
        if (percent < 1)  percent = 1;

        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        // Alert threshold is in bits [4:0] of the low byte, stored as 32 - threshold
        config &= 0xFFE0;  // Clear bits [4:0]
        config |= (32 - percent);
        writeRegister16(MAX17048_REG_CONFIG, config);
        Serial.printf("[BAT] Alert threshold set to %d%%\n", percent);
    }

    // Clear the alert flag (must be done manually after handling)
    static void clearAlert() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config &= ~0x0020;  // Clear ALRT bit (bit 5 of low byte)
        writeRegister16(MAX17048_REG_CONFIG, config);
    }

    // -----------------------------------------------------------------------
    // Deep sleep — unchanged from before
    // -----------------------------------------------------------------------

    static wake_reason_t getWakeReason() {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

        switch (cause) {
            case ESP_SLEEP_WAKEUP_TIMER:
                Serial.println("[PWR] Wake: TIMER");
                return WAKE_TIMER;

            case ESP_SLEEP_WAKEUP_EXT0:
                Serial.println("[PWR] Wake: IMU interrupt (possible fall)");
                return WAKE_FALL_IMU;

            case ESP_SLEEP_WAKEUP_EXT1: {
                uint64_t mask = esp_sleep_get_ext1_wakeup_status();
                if (mask & (1ULL << PANIC_BUTTON_PIN)) {
                    Serial.println("[PWR] Wake: PANIC BUTTON");
                    return WAKE_PANIC;
                }
                Serial.println("[PWR] Wake: EXT1 (unknown pin)");
                return WAKE_EXT1;
            }

            default:
                Serial.println("[PWR] Wake: POWER-ON / RESET");
                return WAKE_UNKNOWN;
        }
    }

    static void enterDeepSleep() {
        Serial.println("[PWR] Preparing for deep sleep...");

        #if MAX17048_SLEEP_WITH_ESP
        sleepFuelGauge();
        #endif

        // --- CRITICAL: Drain pending BNO086 interrupts ---
        // The INT pin is active-LOW and latched. If we configure ext0 wake
        // while INT is already LOW, the ESP32 wakes up immediately.
        // Read all pending sensor events until INT goes HIGH.
        pinMode(BNO086_INT, INPUT_PULLUP);
        if (digitalRead(BNO086_INT) == LOW) {
            Serial.println("[PWR] BNO086 INT is LOW — draining events...");
            // Quick I2C reads to clear the interrupt
            Wire.begin(BNO086_SDA, BNO086_SCL);
            Wire.setClock(400000);
            unsigned long drainStart = millis();
            while (digitalRead(BNO086_INT) == LOW && (millis() - drainStart) < 2000) {
                // Read and discard from the BNO086 to clear its output buffer
                Wire.requestFrom((uint8_t)BNO086_I2C_ADDR, (uint8_t)32);
                while (Wire.available()) Wire.read();
                delay(10);
            }
            if (digitalRead(BNO086_INT) == HIGH) {
                Serial.println("[PWR] INT cleared successfully");
            } else {
                Serial.println("[PWR] INT still LOW after drain — skipping ext0 wake");
            }
        }

        Serial.flush();

        // --- WAKE SOURCE 1: Timer ---
        esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_INTERVAL_SEC * 1000000ULL);
        Serial.printf("[PWR]   Timer wake in %d seconds\n", SLEEP_INTERVAL_SEC);

        // --- WAKE SOURCE 2: BNO086 INT pin (ext0) ---
        // Only enable if INT is currently HIGH (idle), otherwise we'd wake instantly
        if (digitalRead(BNO086_INT) == HIGH) {
            rtc_gpio_pullup_en((gpio_num_t)BNO086_INT);
            rtc_gpio_pulldown_dis((gpio_num_t)BNO086_INT);
            esp_sleep_enable_ext0_wakeup((gpio_num_t)BNO086_INT, 0);
            Serial.println("[PWR]   EXT0 wake on BNO086 INT (LOW)");
        } else {
            Serial.println("[PWR]   EXT0 SKIPPED — INT stuck LOW");
        }

        // --- WAKE SOURCE 3: Panic button (ext1) ---
        uint64_t button_mask = (1ULL << PANIC_BUTTON_PIN);
        esp_sleep_enable_ext1_wakeup(button_mask, ESP_EXT1_WAKEUP_ANY_LOW);
        rtc_gpio_pullup_en((gpio_num_t)PANIC_BUTTON_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)PANIC_BUTTON_PIN);
        Serial.println("[PWR]   EXT1 wake on panic button (LOW)");

        Serial.println("[PWR] Entering deep sleep now.");
        Serial.flush();
        delay(10);

        esp_deep_sleep_start();
    }

private:
    static inline bool gaugeReady = false;

    // -----------------------------------------------------------------------
    // Raw I2C register access for MAX17048
    // -----------------------------------------------------------------------

    static uint16_t readRegister16(uint8_t reg) {
        Wire1.beginTransmission(MAX17048_I2C_ADDR);
        Wire1.write(reg);
        if (Wire1.endTransmission(false) != 0) return 0xFFFF;

        Wire1.requestFrom((uint8_t)MAX17048_I2C_ADDR, (uint8_t)2);
        if (Wire1.available() < 2) return 0xFFFF;

        uint16_t val = ((uint16_t)Wire1.read() << 8) | Wire1.read();
        return val;
    }

    static bool writeRegister16(uint8_t reg, uint16_t value) {
        Wire1.beginTransmission(MAX17048_I2C_ADDR);
        Wire1.write(reg);
        Wire1.write((uint8_t)(value >> 8));
        Wire1.write((uint8_t)(value & 0xFF));
        return (Wire1.endTransmission() == 0);
    }
};

#endif // POWER_H
