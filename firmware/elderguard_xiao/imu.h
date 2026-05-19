#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "espnow_mirror.h"

// ============================================================================
// MPU6500 IMU — Detecção de Queda Multi-Fase + Wake-on-Motion
//
// Inalterado em relação ao projeto original, exceto:
//   • Wire.begin() usa I2C_SDA / I2C_SCL do config.h (barramento compartilhado)
// ============================================================================

// --- Registros MPU6500 ---
#define MPU_ADDR              0x68
#define REG_PWR_MGMT_1        0x6B
#define REG_PWR_MGMT_2        0x6C
#define REG_SMPLRT_DIV        0x19
#define REG_CONFIG            0x1A
#define REG_GYRO_CONFIG       0x1B
#define REG_ACCEL_CONFIG      0x1C
#define REG_ACCEL_CONFIG2     0x1D
#define REG_LP_ACCEL_ODR      0x1E
#define REG_WOM_THR           0x1F
#define REG_INT_PIN_CFG       0x37
#define REG_INT_ENABLE        0x38
#define REG_INT_STATUS        0x3A
#define REG_ACCEL_XOUT_H      0x3B
#define REG_MOT_DETECT_CTRL   0x69
#define REG_SIGNAL_PATH_RST   0x68
#define REG_WHO_AM_I          0x75

// --- Parâmetros do algoritmo ---
#define FF_THRESH_G           0.4f
#define FF_MIN_MS             30
#define IMPACT_THRESH_G       2.0f
#define IMPACT_ROT_MIN_DPS   50.0f
#define FF_TO_IMPACT_MAX_MS   1000
#define INACT_LOW_G           0.8f
#define INACT_HIGH_G          1.2f
#define INACT_MIN_MS          1500
#define ORIENT_CHANGE_G       0.7f
#define ANALYSIS_WINDOW_MS    5000
#define IIR_ALPHA             0.2f
#define INACT_AVG_SIZE        10
#define SAMPLE_RATE_HZ        100
#define SAMPLE_PERIOD_US      (1000000 / SAMPLE_RATE_HZ)

struct SensorData {
    float ax, ay, az;
    float gx, gy, gz;
    float svm;
    float angVelMag;
};

class IMUManager {
public:
    bool begin() {
        // Wire já pode estar inicializado; begin() é idempotente no Arduino
        Wire.begin(IMU_SDA, IMU_SCL);
        Wire.setClock(400000);

        writeReg(REG_PWR_MGMT_1, 0x80);  // Reset completo
        delay(100);

        uint8_t who = readReg(REG_WHO_AM_I);
        if (who != 0x70 && who != 0x73 && who != 0x71) {
            out.printf("[IMU] MPU6500 não encontrado (WHO_AM_I=0x%02X)\n", who);
            return false;
        }
        out.printf("[IMU] MPU6500 OK (0x%02X)\n", who);

        writeReg(REG_PWR_MGMT_1, 0x01);  // Clock PLL
        delay(10);
        writeReg(REG_SIGNAL_PATH_RST, 0x07);
        delay(10);
        return true;
    }

    void enableFallDetectionReports() {
        writeReg(REG_PWR_MGMT_1, 0x01);
        delay(5);
        writeReg(REG_PWR_MGMT_2, 0x00);
        delay(10);
        writeReg(REG_SMPLRT_DIV, 9);        // 100 Hz
        writeReg(REG_CONFIG, 0x04);          // DLPF gyro 20 Hz
        writeReg(REG_ACCEL_CONFIG, 0x10);    // ±8 g
        writeReg(REG_ACCEL_CONFIG2, 0x04);   // DLPF accel 21.2 Hz
        writeReg(REG_GYRO_CONFIG, 0x10);     // ±1000 °/s
        writeReg(REG_INT_ENABLE, 0x00);
        delay(50);
        out.println("[IMU] Full-rate: 100Hz, ±8g, ±1000dps, DLPF 20Hz");
    }

    void enableWakeOnMotion() {
        writeReg(REG_PWR_MGMT_1, 0x01);
        delay(5);
        writeReg(REG_PWR_MGMT_2, 0x07);      // Gyro standby
        writeReg(REG_ACCEL_CONFIG, 0x10);
        writeReg(REG_ACCEL_CONFIG2, 0x01);    // DLPF 184 Hz
        writeReg(REG_INT_PIN_CFG, 0xB0);      // Active LOW, latch, any-read-clear
        writeReg(REG_INT_ENABLE, 0x40);        // WOM_EN
        writeReg(REG_MOT_DETECT_CTRL, 0xC0);
        writeReg(REG_WOM_THR, WOM_THRESHOLD);
        writeReg(REG_LP_ACCEL_ODR, 0x06);     // ~15.63 Hz
        writeReg(REG_PWR_MGMT_1, 0x21);       // CYCLE=1
        readReg(REG_INT_STATUS);
        out.printf("[IMU] WOM ativo (limiar %d = ~%d mg)\n", WOM_THRESHOLD, WOM_THRESHOLD * 4);
    }

    struct FallState {
        float peak_accel = 0;
        float peak_gyro  = 0;
        bool  fall_confirmed = false;
    } fall;

    bool detectFall() {
        firstSample = true;
        for (int i = 0; i < 5; i++) {
            SensorData raw;
            readSensors(raw);
            applyIIR(raw);
            delayMicroseconds(SAMPLE_PERIOD_US);
        }
        refAx = filtered.ax;
        refAy = filtered.ay;
        refAz = filtered.az;

        enum Phase { IDLE, FREEFALL, IMPACT, INACTIVITY, CONFIRMED, NO_FALL };
        Phase phase = IDLE;

        uint32_t tStart = millis();
        uint32_t tFFStart = 0, tFFEnd = 0, tInactStart = 0;
        bool ffDetected = false, impactDetected = false;

        fall.peak_accel = 0;
        fall.peak_gyro  = 0;
        fall.fall_confirmed = false;

        float svmBuf[INACT_AVG_SIZE] = {0};
        uint8_t svmIdx = 0;
        bool svmFull = false;

        while ((millis() - tStart) < ANALYSIS_WINDOW_MS) {
            SensorData raw;
            readSensors(raw);
            applyIIR(raw);

            float svm    = filtered.svm;
            float angVel = filtered.angVelMag;
            uint32_t now = millis();

            svmBuf[svmIdx] = svm;
            svmIdx = (svmIdx + 1) % INACT_AVG_SIZE;
            if (svmIdx == 0) svmFull = true;

            if (svm > fall.peak_accel) fall.peak_accel = svm;
            if (angVel > fall.peak_gyro) fall.peak_gyro = angVel;

            if (!ffDetected) {
                if (svm < FF_THRESH_G) {
                    if (tFFStart == 0) tFFStart = now;
                    if ((now - tFFStart) >= FF_MIN_MS) {
                        ffDetected = true; tFFEnd = now; phase = FREEFALL;
                        out.printf("[FALL] F1: Free-fall SVM=%.2fg %lums\n", svm, now - tFFStart);
                    }
                } else { tFFStart = 0; }
            }

            if (ffDetected && !impactDetected) {
                if ((now - tFFEnd) > FF_TO_IMPACT_MAX_MS) {
                    out.println("[FALL] F2: Timeout sem impacto"); phase = NO_FALL; break;
                }
                if (svm > IMPACT_THRESH_G && angVel > IMPACT_ROT_MIN_DPS) {
                    impactDetected = true; phase = IMPACT;
                    out.printf("[FALL] F2: Impacto SVM=%.2fg w=%.1fdps\n", svm, angVel);
                }
            }

            if (impactDetected && phase == IMPACT) {
                int count = svmFull ? INACT_AVG_SIZE : svmIdx;
                if (count == 0) count = 1;
                float avg = 0;
                for (int i = 0; i < count; i++) avg += svmBuf[i];
                avg /= count;
                if (avg > INACT_LOW_G && avg < INACT_HIGH_G) {
                    if (tInactStart == 0) tInactStart = now;
                    if ((now - tInactStart) >= INACT_MIN_MS) {
                        phase = INACTIVITY;
                        out.printf("[FALL] F3: Inativo SVM_avg=%.2fg\n", avg);
                    }
                } else { tInactStart = 0; }
            }

            if (phase == INACTIVITY) {
                float dx = filtered.ax - refAx;
                float dy = filtered.ay - refAy;
                float dz = filtered.az - refAz;
                float delta = sqrtf(dx*dx + dy*dy + dz*dz);
                if (delta > ORIENT_CHANGE_G) {
                    fall.fall_confirmed = true;
                    out.printf("[FALL] F4: Orientação Δ=%.2fg — QUEDA CONFIRMADA\n", delta);
                    return true;
                }
            }
            delayMicroseconds(SAMPLE_PERIOD_US);
        }
        return false;
    }

    void resetFallState()          { fall = FallState{}; }
    float getLastPeakAccel() const { return fall.peak_accel * 9.80665f; }
    float getLastPeakGyro()  const { return fall.peak_gyro; }

private:
    SensorData filtered;
    bool firstSample = true;
    float refAx = 0, refAy = 0, refAz = -1.0f;

    void readSensors(SensorData& d) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(REG_ACCEL_XOUT_H);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14);
        if (Wire.available() < 14) { d = {}; return; }

        int16_t rax = (Wire.read()<<8)|Wire.read();
        int16_t ray = (Wire.read()<<8)|Wire.read();
        int16_t raz = (Wire.read()<<8)|Wire.read();
        Wire.read(); Wire.read();
        int16_t rgx = (Wire.read()<<8)|Wire.read();
        int16_t rgy = (Wire.read()<<8)|Wire.read();
        int16_t rgz = (Wire.read()<<8)|Wire.read();

        const float aS = 1.0f / 4096.0f;
        const float gS = 1.0f / 32.8f;
        d.ax = rax*aS; d.ay = ray*aS; d.az = raz*aS;
        d.gx = rgx*gS; d.gy = rgy*gS; d.gz = rgz*gS;
        d.svm = sqrtf(d.ax*d.ax + d.ay*d.ay + d.az*d.az);
        d.angVelMag = sqrtf(d.gx*d.gx + d.gy*d.gy + d.gz*d.gz);
    }

    void applyIIR(const SensorData& raw) {
        if (firstSample) { filtered = raw; firstSample = false; return; }
        filtered.ax += IIR_ALPHA * (raw.ax - filtered.ax);
        filtered.ay += IIR_ALPHA * (raw.ay - filtered.ay);
        filtered.az += IIR_ALPHA * (raw.az - filtered.az);
        filtered.gx += IIR_ALPHA * (raw.gx - filtered.gx);
        filtered.gy += IIR_ALPHA * (raw.gy - filtered.gy);
        filtered.gz += IIR_ALPHA * (raw.gz - filtered.gz);
        filtered.svm = sqrtf(filtered.ax*filtered.ax + filtered.ay*filtered.ay + filtered.az*filtered.az);
        filtered.angVelMag = sqrtf(filtered.gx*filtered.gx + filtered.gy*filtered.gy + filtered.gz*filtered.gz);
    }

    void writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(reg); Wire.write(val);
        Wire.endTransmission();
    }

    uint8_t readReg(uint8_t reg) {
        Wire.beginTransmission(MPU_ADDR);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1);
        return Wire.available() ? Wire.read() : 0xFF;
    }
};

#endif // IMU_H
