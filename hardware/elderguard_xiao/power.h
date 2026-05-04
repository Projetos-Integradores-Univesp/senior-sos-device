#ifndef POWER_H
#define POWER_H

#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "config.h"
#include "espnow_mirror.h"
#include "ulp_blink.h"

// alertLedActive é declarada como RTC_DATA_ATTR no sketch principal.
// Power.h a lê para decidir se arma o ULP antes de dormir.
extern bool alertLedActive;

#ifndef MPU6500_ADDR
#define MPU6500_ADDR 0x68
#endif

// ============================================================================
// PowerManager — XIAO ESP32S3
//
// Mudanças em relação ao projeto original (A7670E):
//   • MAX17048 agora compartilha o barramento Wire com o MPU6500
//     (não usa Wire1). Wire já é iniciado pelo IMUManager; PowerManager
//     assume que Wire.begin() foi chamado antes de beginFuelGauge().
//   • getSleepInterval() retorna 900 s (normal) ou 1800 s (crítico),
//     lendo o SoC ao vivo sem custo extra — o gauge já está acordado.
//   • Removidas todas as referências ao modem A7670E / GPS.
// ============================================================================

class PowerManager {
public:

    // -----------------------------------------------------------------------
    // Inicialização do fuel gauge
    // Pré-condição: Wire.begin(I2C_SDA, I2C_SCL) já executado (pelo IMUManager)
    // -----------------------------------------------------------------------
    static bool beginFuelGauge() {
        // Wire já iniciado — apenas verificar velocidade
        Wire.setClock(400000);
        delay(50);

        uint16_t version = 0;
        for (int attempt = 0; attempt < 5; attempt++) {
            version = readRegister16(MAX17048_REG_VERSION);
            if (version != 0 && version != 0xFFFF) break;
            delay(100);
        }

        if (version == 0 || version == 0xFFFF) {
            out.println("[BAT] MAX17048 não encontrado (SDA=" + String(I2C_SDA) + " SCL=" + String(I2C_SCL) + ")");
            return false;
        }
        out.printf("[BAT] MAX17048 encontrado, version: 0x%04X\n", version);

        quickStart();
        delay(600);   // ModelGauge precisa de ~500 ms após quick-start

        gaugeReady = true;
        validateAndFixSoC();
        setAlertThreshold(MAX17048_ALERT_THRESHOLD);
        clearAlert();
        return true;
    }

    // -----------------------------------------------------------------------
    // Leituras de bateria
    // -----------------------------------------------------------------------
    static float readBatteryVoltage() {
        if (!gaugeReady && !beginFuelGauge()) return 0.0f;
        uint16_t raw = readRegister16(MAX17048_REG_VCELL);
        return raw * 78.125e-6f;
    }

    static float readBatterySOC() {
        if (!gaugeReady && !beginFuelGauge()) return -1.0f;
        uint16_t raw = readRegister16(MAX17048_REG_SOC);
        if (raw == 0xFFFF) return -1.0f;
        float soc = (raw >> 8) + (raw & 0xFF) / 256.0f;
        if (soc > 100.0f) soc = 100.0f;
        if (soc < 0.0f)   soc = 0.0f;
        return soc;
    }

    static int batteryPercent() {
        float soc = readBatterySOC();
        if (soc < 0) return -1;
        return (int)(soc + 0.5f);
    }

    static float readChargeRate() {
        if (!gaugeReady) return 0.0f;
        uint16_t raw = readRegister16(MAX17048_REG_CRATE);
        if (raw == 0xFFFF) return 0.0f;
        return (int16_t)raw * 0.208f;
    }

    static bool isAlertActive() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        if (config == 0xFFFF) return false;
        return (config & 0x0020);
    }

    static bool isBatteryCritical() {
        int pct = batteryPercent();
        if (pct < 0) return false;
        if (pct < MAX17048_CRITICAL_PCT) {
            float v = readBatteryVoltage();
            if (v > 3.5f) {
                out.printf("[BAT] SoC=%d%% mas V=%.2fV — falso low, ignorando\n", pct, v);
                return false;
            }
            return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // Intervalo de sleep adaptativo
    //   Normal  → 15 min (900 s)
    //   Crítico → 30 min (1800 s)
    // -----------------------------------------------------------------------
    static uint32_t getSleepInterval() {
        if (isBatteryCritical()) {
            out.printf("[PWR] Carga crítica — heartbeat 30 min\n");
            return SLEEP_INTERVAL_CRITICAL_SEC;
        }
        out.printf("[PWR] Carga normal — heartbeat 15 min\n");
        return SLEEP_INTERVAL_NORMAL_SEC;
    }

    // -----------------------------------------------------------------------
    // Modos de energia do fuel gauge
    // -----------------------------------------------------------------------
    static void sleepFuelGauge() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config |= 0x0080;
        writeRegister16(MAX17048_REG_CONFIG, config);
        writeRegister16(MAX17048_REG_MODE, 0x0000);
        out.println("[BAT] MAX17048 → sleep (~0.5 µA)");
    }

    static void wakeFuelGauge() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config &= ~0x0080;
        writeRegister16(MAX17048_REG_CONFIG, config);
        quickStart();
        out.println("[BAT] MAX17048 acordado, quick-start emitido");
    }

    static void quickStart() {
        writeRegister16(MAX17048_REG_MODE, 0x4000);
        delay(2);
    }

    static void resetGauge() {
        out.println("[BAT] MAX17048 POR reset...");
        writeRegister16(MAX17048_REG_COMMAND, 0x5400);
        delay(200);
        uint16_t version = 0;
        for (int i = 0; i < 10; i++) {
            version = readRegister16(MAX17048_REG_VERSION);
            if (version != 0 && version != 0xFFFF) break;
            delay(100);
        }
        if (version != 0 && version != 0xFFFF) {
            out.printf("[BAT] MAX17048 reset OK, version: 0x%04X\n", version);
            quickStart();
            delay(600);
        } else {
            out.println("[BAT] MAX17048 não respondeu após reset!");
        }
    }

    static void validateAndFixSoC() {
        float v   = readBatteryVoltage();
        float soc = readBatterySOC();
        if (soc < 0) return;

        float est = 0;
        if      (v >= 4.15) est = 95;
        else if (v >= 3.90) est = 70;
        else if (v >= 3.70) est = 40;
        else if (v >= 3.50) est = 15;
        else if (v >= 3.30) est = 5;

        if (fabsf(soc - est) > 40.0f) {
            out.printf("[BAT] SoC=%.0f%% vs est.=%.0f%% (V=%.2fV) — divergência, reset\n", soc, est, v);
            resetGauge();
        }
    }

    static void setAlertThreshold(uint8_t percent) {
        if (percent > 32) percent = 32;
        if (percent < 1)  percent = 1;
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config &= 0xFFE0;
        config |= (32 - percent);
        writeRegister16(MAX17048_REG_CONFIG, config);
        out.printf("[BAT] Alert threshold: %d%%\n", percent);
    }

    static void clearAlert() {
        uint16_t config = readRegister16(MAX17048_REG_CONFIG);
        config &= ~0x0020;
        writeRegister16(MAX17048_REG_CONFIG, config);
    }

    // -----------------------------------------------------------------------
    // Wake-up reason
    // -----------------------------------------------------------------------
    static wake_reason_t getWakeReason() {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        switch (cause) {
            case ESP_SLEEP_WAKEUP_TIMER:
                out.println("[PWR] Wake: TIMER");
                return WAKE_TIMER;
            case ESP_SLEEP_WAKEUP_EXT0:
                out.println("[PWR] Wake: IMU interrupt (possível queda)");
                return WAKE_FALL_IMU;
            case ESP_SLEEP_WAKEUP_EXT1: {
                uint64_t mask = esp_sleep_get_ext1_wakeup_status();
                if (mask & (1ULL << PANIC_BUTTON_PIN)) {
                    out.println("[PWR] Wake: BOTÃO DE PÂNICO");
                    return WAKE_PANIC;
                }
                return WAKE_UNKNOWN;
            }
            default:
                out.println("[PWR] Wake: POWER-ON / RESET");
                return WAKE_UNKNOWN;
        }
    }

    // -----------------------------------------------------------------------
    // Deep sleep
    // -----------------------------------------------------------------------
    static void enterDeepSleep() {
        out.println("[PWR] Preparando deep sleep...");

#if MAX17048_SLEEP_WITH_ESP
        sleepFuelGauge();
#endif

        // Limpar interrupção pendente do MPU6500 (INT ativo-LOW travado)
        pinMode(IMU_INT, INPUT_PULLUP);
        if (digitalRead(IMU_INT) == LOW) {
            out.println("[PWR] MPU6500 INT LOW — limpando...");
            unsigned long t0 = millis();
            while (digitalRead(IMU_INT) == LOW && (millis() - t0) < 2000) {
                Wire.beginTransmission(MPU6500_ADDR);
                Wire.write(0x3A);  // INT_STATUS
                Wire.endTransmission(false);
                Wire.requestFrom((uint8_t)MPU6500_ADDR, (uint8_t)1);
                if (Wire.available()) Wire.read();
                delay(10);
            }
        }

        out.flush();

        // Wake source 1: timer adaptativo
        uint32_t sleepSec = getSleepInterval();
        esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
        out.printf("[PWR]   Timer wake em %u s (%u min)\n", sleepSec, sleepSec / 60);

        // Wake source 2: IMU INT (ext0, active LOW)
        if (digitalRead(IMU_INT) == HIGH) {
            rtc_gpio_pullup_en((gpio_num_t)IMU_INT);
            rtc_gpio_pulldown_dis((gpio_num_t)IMU_INT);
            esp_sleep_enable_ext0_wakeup((gpio_num_t)IMU_INT, 0);
            out.println("[PWR]   EXT0 wake em MPU6500 INT (LOW)");
        } else {
            out.println("[PWR]   EXT0 SKIPPED — INT preso em LOW");
        }

        // Wake source 3: botão de pânico (ext1)
        uint64_t btn_mask = (1ULL << PANIC_BUTTON_PIN);
        esp_sleep_enable_ext1_wakeup(btn_mask, ESP_EXT1_WAKEUP_ANY_LOW);
        rtc_gpio_pullup_en((gpio_num_t)PANIC_BUTTON_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)PANIC_BUTTON_PIN);
        out.println("[PWR]   EXT1 wake em botão de pânico (LOW)");

        out.println("[PWR] Entrando em deep sleep.");
        out.flush();
        delay(10);

        // ---- Armar ULP pisca se LED de alerta estiver ativo ----
        // Chamado após todas as fontes de wake estarem configuradas e
        // imediatamente antes de esp_deep_sleep_start().
        // O ULP pisca de forma autônoma sem acordar o núcleo principal;
        // o núcleo acorda normalmente pelas fontes timer / ext0 / ext1.
        if (alertLedActive) {
            UlpBlink::start();
        }

        esp_deep_sleep_start();
    }

private:
    static inline bool gaugeReady = false;

    // MAX17048 agora usa Wire (mesmo bus do IMU)
    static uint16_t readRegister16(uint8_t reg) {
        Wire.beginTransmission(MAX17048_I2C_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) return 0xFFFF;
        Wire.requestFrom((uint8_t)MAX17048_I2C_ADDR, (uint8_t)2);
        if (Wire.available() < 2) return 0xFFFF;
        return ((uint16_t)Wire.read() << 8) | Wire.read();
    }

    static bool writeRegister16(uint8_t reg, uint16_t value) {
        Wire.beginTransmission(MAX17048_I2C_ADDR);
        Wire.write(reg);
        Wire.write((uint8_t)(value >> 8));
        Wire.write((uint8_t)(value & 0xFF));
        return (Wire.endTransmission() == 0);
    }
};

#endif // POWER_H
