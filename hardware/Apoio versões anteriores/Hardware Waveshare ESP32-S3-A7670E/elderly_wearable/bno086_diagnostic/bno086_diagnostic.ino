// ============================================================================
// BNO086 Diagnostic v2 — test INT pin, I2C, and interrupt clearing
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_BNO08x_Arduino_Library.h>

#define BNO086_SDA   2
#define BNO086_SCL   1
#define BNO086_INT   3
#define BNO086_RST   4
#define BNO086_ADDR  0x4B

BNO08x imu;
bool imuOk = false;

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    delay(2000);
    Serial.println("\n========== BNO086 Diagnostic ==========\n");

    // --- Step 1: Check INT pin state BEFORE any I2C ---
    pinMode(BNO086_INT, INPUT_PULLUP);
    delay(100);
    int intBefore = digitalRead(BNO086_INT);
    Serial.printf("[1] INT pin (GPIO%d) before I2C: %s\n",
                  BNO086_INT, intBefore ? "HIGH (idle)" : "LOW (asserted!)");

    // --- Step 2: Hardware reset FIRST ---
    Serial.println("\n[2] Hardware reset via RST pin...");
    pinMode(BNO086_RST, OUTPUT);
    digitalWrite(BNO086_RST, LOW);
    delay(20);
    digitalWrite(BNO086_RST, HIGH);
    delay(500);

    int intAfterReset = digitalRead(BNO086_INT);
    Serial.printf("    INT after reset: %s\n",
                  intAfterReset ? "HIGH (reset cleared it)" : "LOW (still asserted)");

    // --- Step 3: I2C bus init and scan ---
    Serial.printf("\n[3] I2C scan on SDA=GPIO%d, SCL=GPIO%d\n", BNO086_SDA, BNO086_SCL);
    Wire.begin(BNO086_SDA, BNO086_SCL);
    Wire.setClock(100000);
    delay(100);

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            Serial.printf("    0x%02X", addr);
            if (addr == BNO086_ADDR) Serial.print(" <- BNO086 expected");
            if (addr == 0x4B)        Serial.print(" <- BNO086 alt addr");
            if (addr == 0x36)        Serial.print(" <- MAX17048");
            Serial.println();
            found++;
        }
    }
    if (found == 0) {
        Serial.println("    No devices found!");
        Serial.println("    Check: VCC->3.3V, GND, SDA->GPIO2, SCL->GPIO1");
        Serial.println("    Check: PS0->GND, PS1->GND (I2C mode)");
        Serial.println("    Check: pull-ups 4.7k on SDA and SCL to 3.3V");
        Serial.println("\n========== STOPPING ==========");
        return;
    }

    // --- Step 4: Try to initialise the BNO086 ---
    Serial.println("\n[4] Initialising BNO086...");
    Wire.setClock(400000);
    delay(50);

    imuOk = imu.begin(BNO086_ADDR, Wire);
    if (!imuOk) {
        Serial.println("    FAILED at 0x4A");
        Serial.println("    Trying alternate address 0x4B...");
        imuOk = imu.begin(0x4B, Wire);
        if (!imuOk) {
            Serial.println("    Also failed at 0x4B");
            Serial.println("    I2C device found but SH-2 protocol init failed");
            return;
        }
        Serial.println("    Success at 0x4B! Update BNO086_ADDR in config.h");
    } else {
        Serial.println("    BNO086 OK at 0x4A");
    }

    // --- Step 5: Check INT after init ---
    delay(200);
    int intAfterInit = digitalRead(BNO086_INT);
    Serial.printf("\n[5] INT after init: %s\n",
                  intAfterInit ? "HIGH (cleared)" : "LOW (still asserted)");

    // --- Step 6: Enable report and drain ---
    Serial.println("\n[6] Enabling accelerometer and reading events...");
    imu.enableAccelerometer(50);
    delay(200);

    int eventsRead = 0;
    unsigned long start = millis();
    while (millis() - start < 3000) {
        if (imu.getSensorEvent()) {
            eventsRead++;
            if (eventsRead <= 3) {
                uint8_t id = imu.getSensorEventID();
                Serial.printf("    Event #%d (report 0x%02X)\n", eventsRead, id);
            }
        }
        delay(5);
    }
    Serial.printf("    Total: %d events in 3 seconds\n", eventsRead);

    int intAfterDrain = digitalRead(BNO086_INT);
    Serial.printf("    INT after drain: %s\n",
                  intAfterDrain ? "HIGH (cleared!)" : "LOW (stuck)");

    // --- Summary ---
    Serial.println("\n========== Diagnosis ==========");
    if (!intBefore && intAfterReset) {
        Serial.println("RESULT: INT was stuck from previous session.");
        Serial.println("  Hardware reset cleared it. Chip is fine.");
        Serial.println("  FIX: Add hardware reset before configuring ext0 wake.");
    } else if (!intBefore && !intAfterReset && intAfterInit) {
        Serial.println("RESULT: INT cleared after full SH-2 init.");
        Serial.println("  Chip is fine but needs init to clear pending data.");
        Serial.println("  FIX: Init + drain events before sleep.");
    } else if (!intAfterDrain) {
        Serial.println("RESULT: INT stuck LOW after all attempts.");
        Serial.println("  Possible causes:");
        Serial.println("  1. Missing pull-up on INT (add 10k to 3.3V)");
        Serial.println("  2. Wiring short on GPIO3");
        Serial.println("  3. Damaged BNO086 (unlikely if I2C works)");
        Serial.println("  Try: disconnect INT wire and check if GPIO3 reads HIGH");
    } else if (intBefore && intAfterReset) {
        Serial.println("RESULT: INT is HIGH and behaving normally.");
        Serial.println("  Chip appears healthy.");
    }
    if (eventsRead == 0 && imuOk) {
        Serial.println("WARNING: Init succeeded but no events received.");
        Serial.println("  The sensor may need a power cycle.");
    }
    Serial.println("================================\n");
}

void loop() {
    if (!imuOk) {
        delay(1000);
        return;
    }

    static unsigned long lastPrint = 0;
    static int lastState = -1;

    int state = digitalRead(BNO086_INT);
    if (state != lastState) {
        Serial.printf("[INT] %s\n", state ? "HIGH" : "LOW");
        lastState = state;
    }

    if (imu.getSensorEvent()) {
        uint8_t id = imu.getSensorEventID();
        if (id == SENSOR_REPORTID_ACCELEROMETER && (millis() - lastPrint > 2000)) {
            Serial.printf("[IMU] ax=%.2f ay=%.2f az=%.2f  INT=%s\n",
                          imu.getAccelX(), imu.getAccelY(), imu.getAccelZ(),
                          digitalRead(BNO086_INT) ? "H" : "L");
            lastPrint = millis();
        }
    }
    delay(10);
}
