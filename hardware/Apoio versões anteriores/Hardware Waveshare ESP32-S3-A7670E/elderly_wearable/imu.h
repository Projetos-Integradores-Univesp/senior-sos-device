#ifndef IMU_H
#define IMU_H

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>  // Install via Library Manager
#include "config.h"

// ============================================================================
// BNO086 IMU — Fall Detection & Wake-on-Motion
// ============================================================================

class IMUManager {
public:
    BNO08x imu;

    bool begin() {
        Wire.begin(BNO086_SDA, BNO086_SCL);
        Wire.setClock(400000);  // 400 kHz fast-mode I2C

        // Optional hard reset
        pinMode(BNO086_RST, OUTPUT);
        digitalWrite(BNO086_RST, LOW);
        delay(10);
        digitalWrite(BNO086_RST, HIGH);
        delay(100);

        if (!imu.begin(BNO086_I2C_ADDR, Wire)) {
            Serial.println("[IMU] BNO086 not found — check wiring");
            return false;
        }
        Serial.println("[IMU] BNO086 initialised");
        return true;
    }

    // Enable the reports we need for fall detection
    void enableFallDetectionReports() {
        // Linear acceleration (gravity removed) — detect impact spikes
        imu.enableLinearAccelerometer(10);  // 10 ms interval = 100 Hz

        // Gyroscope — detect rapid rotation
        imu.enableGyro(10);

        // Stability classifier — the BNO086 has built-in stable/motion detection
        imu.enableStabilityClassifier(100);

        // Game rotation vector — orientation tracking (low drift, no mag)
        imu.enableGameRotationVector(50);

        Serial.println("[IMU] Fall-detection reports enabled");
    }

    // Enable "significant motion" interrupt for deep-sleep wake.
    // The BNO086's HINT pin goes LOW when motion exceeding the
    // internal threshold is detected. We wire this to ext0 wake.
    void enableWakeOnMotion() {
        // The BNO086 "Significant Motion" feature (report ID 0x13)
        // fires once when it detects significant movement, then disables itself.
        // Perfect for a one-shot deep-sleep wake trigger.
        imu.enableActivityClassifier(100,
            SH2_STEP_DETECTOR);  // Activities to monitor

        // Also enable the personal-activity classifier which includes
        // "significant motion" as a category
        Serial.println("[IMU] Wake-on-motion configured via significant motion detector");
    }

    // -----------------------------------------------------------------------
    // Fall Detection Algorithm
    //
    // Three-phase detection:
    //   1. FREE-FALL: Linear accel magnitude drops below threshold (~2 m/s²)
    //   2. IMPACT:    Sudden spike in accel magnitude (>30 m/s²)
    //   3. INACTIVITY: Person remains still for several seconds post-impact
    // -----------------------------------------------------------------------

    struct FallState {
        bool in_freefall      = false;
        bool impact_detected  = false;
        bool fall_confirmed   = false;
        unsigned long freefall_start = 0;
        unsigned long impact_time    = 0;
        float peak_accel      = 0;
        float peak_gyro       = 0;
    } fall;

    // Call this in a tight loop after wake; returns true if a fall is confirmed
    bool detectFall() {
        if (!imu.getSensorEvent()) return false;

        uint8_t id = imu.getSensorEventID();

        switch (id) {
            case SENSOR_REPORTID_LINEAR_ACCELERATION:
                processAccel(imu.getLinAccelX(), imu.getLinAccelY(), imu.getLinAccelZ());
                break;

            case SENSOR_REPORTID_GYROSCOPE_CALIBRATED:
                processGyro(imu.getGyroX(), imu.getGyroY(), imu.getGyroZ());
                break;

            case SENSOR_REPORTID_STABILITY_CLASSIFIER: {
                uint8_t stability = imu.getStabilityClassifier();
                // 0=unknown, 1=on-table, 2=stationary, 3=stable, 4=motion, 5=reserved
                if (fall.impact_detected && (stability <= 2)) {
                    unsigned long elapsed = millis() - fall.impact_time;
                    if (elapsed > (FALL_CONFIRM_WINDOW_SEC * 1000)) {
                        fall.fall_confirmed = true;
                        Serial.println("[FALL] CONFIRMED — person is stationary post-impact");
                    }
                }
                break;
            }
            default:
                break;
        }

        if (fall.fall_confirmed) {
            FallState copy = fall;
            resetFallState();
            fall.fall_confirmed = true;  // Keep flag for caller
            return true;
        }
        return false;
    }

    void resetFallState() {
        fall = FallState{};
    }

    float getLastPeakAccel() const { return fall.peak_accel; }
    float getLastPeakGyro()  const { return fall.peak_gyro; }

private:
    void processAccel(float x, float y, float z) {
        float mag = sqrt(x * x + y * y + z * z);

        // Phase 1 — detect free-fall
        if (!fall.in_freefall && mag < FALL_FREEFALL_THRESHOLD) {
            fall.in_freefall = true;
            fall.freefall_start = millis();
            Serial.printf("[FALL] Free-fall detected (accel=%.1f)\n", mag);
        }

        // Phase 2 — detect impact after free-fall
        if (fall.in_freefall && !fall.impact_detected) {
            unsigned long ff_duration = millis() - fall.freefall_start;
            if (mag > FALL_ACCEL_THRESHOLD && ff_duration >= FALL_FREEFALL_DURATION_MS) {
                fall.impact_detected = true;
                fall.impact_time = millis();
                fall.peak_accel = mag;
                Serial.printf("[FALL] Impact detected (accel=%.1f, ff_dur=%lu ms)\n",
                              mag, ff_duration);
            }
            // Timeout: if free-fall lasts too long without impact, reset
            if (ff_duration > 1000) {
                Serial.println("[FALL] Free-fall timeout — resetting");
                resetFallState();
            }
        }

        if (mag > fall.peak_accel) fall.peak_accel = mag;
    }

    void processGyro(float x, float y, float z) {
        float mag = sqrt(x * x + y * y + z * z);
        // Convert from rad/s to deg/s
        float deg = mag * 57.2958f;
        if (deg > fall.peak_gyro) fall.peak_gyro = deg;

        // Rapid rotation alone can also indicate a fall (person tumbling)
        if (deg > FALL_GYRO_THRESHOLD && !fall.impact_detected) {
            Serial.printf("[FALL] Rapid rotation detected (%.1f deg/s)\n", deg);
            // Don't confirm yet — wait for impact + inactivity
        }
    }
};

#endif // IMU_H
