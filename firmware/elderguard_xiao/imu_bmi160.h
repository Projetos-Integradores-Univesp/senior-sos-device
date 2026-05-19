#ifndef IMU_BMI160_H
#define IMU_BMI160_H

// ============================================================================
// BMI160 IMU — Variante para o ElderGuard XIAO ESP32S3
// Drop-in replacement para imu.h (MPU6500)
//
// Para usar: substituir  #include "imu.h"  por  #include "imu_bmi160.h"
// A classe IMUManager e todos os métodos públicos são idênticos.
// Nenhuma outra alteração no firmware é necessária.
//
// DIFERENÇAS FUNDAMENTAIS BMI160 × MPU6500
// ─────────────────────────────────────────────────────────────────────────────
// Endereço I2C:
//   BMI160: 0x68 (SDO/SA0 = GND) ou 0x69 (SDO/SA0 = VCC)  — mesmo que MPU6500
//
// Modelo de power modes:
//   MPU6500: modo CYCLE (wake-on-motion hardware WOM com accel LP)
//   BMI160:  modo LOW POWER (duty-cycling accel suspend ↔ normal).
//     O BMI160 NÃO tem um WOM dedicado como o MPU6500.
//     O equivalente é configurar a interrupção ANY-MOTION com o acelerômetro
//     em Low Power Mode (CMD 0x12 para ACC_LP_MODE).
//     Consumo em LP + any-motion: ~3–6 µA típico @ 25 Hz ODR.
//     (MPU6500 WOM: ~8 µA @ 16 Hz)
//
// Gyroscópio em sleep:
//   BMI160: giroscópio não tem low-power mode. Vai para SUSPEND (CMD 0x14).
//   MPU6500: gyro standby via PWR_MGMT_2.
//
// Limpeza de interrupção:
//   BMI160: leitura de INT_STATUS (0x1C–0x1F) limpa o latch automaticamente
//     quando int_latch = 0 (modo temporário/non-latched).
//   MPU6500: leitura de INT_STATUS (0x3A).
//
// Escala do acelerômetro:
//   BMI160 ±8g: 4096 LSB/g   (mesmo que MPU6500 ±8g)
//   BMI160 ±16g: 2048 LSB/g
//
// Registradores de dados:
//   BMI160: accel em 0x12–0x17 (order: X_L, X_H, Y_L, Y_H, Z_L, Z_H)
//           gyro  em 0x0C–0x11 (order: X_L, X_H, Y_L, Y_H, Z_L, Z_H)
//   MPU6500: dados em burst a partir de 0x3B (accel → temp → gyro, big-endian)
//
// Chip ID:
//   BMI160: WHO_AM_I (0x00) = 0xD1
//   MPU6500: WHO_AM_I (0x75) = 0x70 / 0x73 / 0x71
//
// Pino INT:
//   Mesmo GPIO3 do XIAO. No BMI160 o pino é INT1 ou INT2 (breakout GY-BMI160
//   tipicamente expõe INT1). Configurar como active LOW, latched temporário.
//
// ALGORITMO DE DETECÇÃO DE QUEDA:
//   Idêntico ao MPU6500 — 4 fases (free-fall → impacto → inatividade →
//   mudança de orientação), filtro IIR, médias móveis. Os thresholds em 'g'
//   são os mesmos. Apenas a configuração de hardware mudou.
//
// REFERÊNCIAS DE REGISTRO:
//   Tabela conforme BMI160 Datasheet BST-BMI160-DS000-09 (Nov 2020)
// ============================================================================

#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "espnow_mirror.h"

// ─── Endereço I2C ────────────────────────────────────────────────────────────
// GY-BMI160: SDO → GND por padrão → 0x68
// Se o seu módulo tem SDO/SA0 em VCC, trocar para 0x69
#define BMI_ADDR              0x68

// ─── Registradores principais (BMI160 Datasheet §2.11) ───────────────────────
#define BMI_REG_CHIP_ID       0x00   // Deve retornar 0xD1
#define BMI_REG_ERR_REG       0x02
#define BMI_REG_PMU_STATUS    0x03   // Estado atual dos modos de energia
#define BMI_REG_GYR_X_L       0x0C   // Gyro data (little-endian)
#define BMI_REG_ACC_X_L       0x12   // Accel data (little-endian)
#define BMI_REG_INT_STATUS_0  0x1C   // Fonte de interrupção (any-motion = bit 2)
#define BMI_REG_INT_STATUS_1  0x1D
#define BMI_REG_ACC_CONF      0x40   // ODR + BW do acelerômetro
#define BMI_REG_ACC_RANGE     0x41   // Fundo de escala do acelerômetro
#define BMI_REG_GYR_CONF      0x42   // ODR + BW do giroscópio
#define BMI_REG_GYR_RANGE     0x43   // Fundo de escala do giroscópio
#define BMI_REG_INT_EN_0      0x50   // Habilitação de interrupções grupo 0
#define BMI_REG_INT_EN_1      0x51   // Habilitação de interrupções grupo 1
#define BMI_REG_INT_EN_2      0x52   // Habilitação de interrupções grupo 2
#define BMI_REG_INT_OUT_CTRL  0x53   // Configuração elétrica dos pinos INT
#define BMI_REG_INT_LATCH     0x54   // Modo de latch da interrupção
#define BMI_REG_INT_MAP_0     0x55   // Mapeamento de interrupções → INT1
#define BMI_REG_INT_MAP_1     0x56   // Mapeamento de interrupções → INT1/INT2
#define BMI_REG_INT_MAP_2     0x57   // Mapeamento de interrupções → INT2
#define BMI_REG_INT_MOTION_0  0x5F   // any_motion_dur (número de amostras)
#define BMI_REG_INT_MOTION_1  0x60   // any_motion_thr (limiar)
#define BMI_REG_LOW_HIGH_0    0x5A   // low_g_dur
#define BMI_REG_LOW_HIGH_1    0x5B   // low_g_thr
#define BMI_REG_LOW_HIGH_2    0x5C   // low_g_hyst + low_g_mode + high_g_hyst
#define BMI_REG_LOW_HIGH_3    0x5D   // high_g_thr
#define BMI_REG_LOW_HIGH_4    0x5E   // high_g_dur
#define BMI_REG_CMD           0x7E   // Registro de comandos

// ─── Comandos para o registro CMD (0x7E) ─────────────────────────────────────
#define BMI_CMD_SOFTRESET     0xB6   // Reset completo
#define BMI_CMD_ACC_NORMAL    0x11   // Acelerômetro → normal mode
#define BMI_CMD_ACC_LP        0x12   // Acelerômetro → low power mode
#define BMI_CMD_ACC_SUSPEND   0x10   // Acelerômetro → suspend
#define BMI_CMD_GYR_NORMAL    0x15   // Giroscópio → normal mode
#define BMI_CMD_GYR_FSTART    0x17   // Giroscópio → fast start-up mode
#define BMI_CMD_GYR_SUSPEND   0x14   // Giroscópio → suspend

// ─── Valores de configuração ──────────────────────────────────────────────────
// ACC_CONF (0x40): bits[3:0] = ODR, bit7 = acc_us (1 = low power mode ativo)
//   ODR 0x07 = 50 Hz (análise de queda)
//   ODR 0x06 = 25 Hz (monitoramento LP)
//   acc_bwp (bits [6:4]): 0b010 = avg4 (recomendado para LP)
#define BMI_ACC_CONF_NORMAL   0x27   // ODR=100Hz, bwp=normal (análise de queda)
#define BMI_ACC_CONF_LP       0xA6   // acc_us=1, ODR=25Hz, bwp=avg4 (monit. LP)

// ACC_RANGE (0x41): 0x08 = ±8g (4096 LSB/g) — mesmo que MPU6500
#define BMI_ACC_RANGE_8G      0x08

// GYR_CONF (0x42): ODR=200Hz, bwp=normal
#define BMI_GYR_CONF_NORMAL   0x28   // ODR=200Hz
// GYR_RANGE (0x43): 0x01 = ±1000°/s (32.8 LSB/°/s)
#define BMI_GYR_RANGE_1000    0x01

// INT_OUT_CTRL (0x53): INT1 active LOW, push-pull, enabled
//   int1_lvl=0 (LOW), int1_od=0 (push-pull), int1_output_en=1
#define BMI_INT1_ACTIVE_LOW   0x08   // bit3=int1_output_en, bit1=int1_lvl(0=LOW)

// INT_LATCH (0x54):
//   Bit 3 (int_input_en): 0 = INT1 como saída
//   Bits[3:0]: latch mode — 0b0000 = non-latched (limpa ao ler INT_STATUS)
//   Usar non-latched para que a próxima interrupção seja detectada rapidamente
#define BMI_INT_LATCH_NONLATCH  0x00

// INT_EN_0 (0x50): bit6 = anymotion_x_en, bit5 = anymotion_y_en, bit4 = anymotion_z_en
#define BMI_INT_EN0_ANYMOTION 0x07   // Habilita any-motion em X, Y e Z

// INT_MAP_0 (0x55): bit2 = int1_anymotion → mapeia any-motion para INT1
#define BMI_MAP0_ANYMOTION_INT1  0x04

// INT_MOTION_0 (0x5F): any_motion_dur (bits[1:0]) = número de amostras consecutivas
//   00 = 1 amostra, 01 = 2 amostras, 10 = 3 amostras, 11 = 4 amostras
//   Usar 01 (2 amostras) para reduzir falsos alarmes por ruído
#define BMI_ANYMOTION_DUR_2   0x01

// INT_MOTION_1 (0x60): any_motion_thr
//   Limiar em unidades de (2 * acc_range / 512) g/LSB para ±8g
//   ±8g → 1 LSB = 2*8/512 = 0.03125 g ≈ 31.25 mg
//   Para ~600 mg (equivalente ao WOM_THRESHOLD=150 do MPU6500 = 150×4=600mg):
//   600 mg / 31.25 mg = 19.2 → usar 20 (≈ 625 mg)
#define BMI_ANYMOTION_THR     20     // ≈ 625 mg

// ─── Parâmetros do algoritmo de queda (idênticos ao MPU6500) ─────────────────
#define FF_THRESH_G           0.4f
#define FF_MIN_MS             30
#define IMPACT_THRESH_G       2.0f
#define IMPACT_ROT_MIN_DPS    50.0f
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

// ─── Escalas ─────────────────────────────────────────────────────────────────
// ±8g   → 4096 LSB/g   (igual ao MPU6500 em ±8g)
// ±1000°/s → 32.8 LSB/°/s (igual ao MPU6500 em ±1000°/s)
static constexpr float BMI_ACCEL_SCALE = 1.0f / 4096.0f;
static constexpr float BMI_GYRO_SCALE  = 1.0f / 32.8f;

struct SensorData {
    float ax, ay, az;
    float gx, gy, gz;
    float svm;
    float angVelMag;
};

class IMUManager {
public:
    // -------------------------------------------------------------------------
    // begin() — inicializa o BMI160, confirma chip ID
    // -------------------------------------------------------------------------
    bool begin() {
        Wire.begin(IMU_SDA, IMU_SCL);
        Wire.setClock(400000);

        // Soft reset — todos os registradores voltam ao padrão
        writeReg(BMI_REG_CMD, BMI_CMD_SOFTRESET);
        delay(10);   // ≥ 1 ms após softreset (datasheet §2.3)

        // Dummy read em 0x7F após reset (requisito do protocolo I2C do BMI160)
        readReg(0x7F);
        delay(2);

        uint8_t chipId = readReg(BMI_REG_CHIP_ID);
        if (chipId != 0xD1) {
            out.printf("[IMU] BMI160 não encontrado (CHIP_ID=0x%02X, esperado 0xD1)\n", chipId);
            return false;
        }
        out.printf("[IMU] BMI160 OK (CHIP_ID=0x%02X)\n", chipId);

        // Acordar acelerômetro em normal mode (necessário antes de configurar)
        writeReg(BMI_REG_CMD, BMI_CMD_ACC_NORMAL);
        delay(5);

        // Acordar giroscópio em normal mode
        writeReg(BMI_REG_CMD, BMI_CMD_GYR_NORMAL);
        delay(85);   // ≥ 80 ms para estabilização do gyro (datasheet §2.2.3)

        return true;
    }

    // -------------------------------------------------------------------------
    // enableFallDetectionReports()
    // Configura accel + gyro em modo normal a alta taxa para análise de queda.
    // Chamado imediatamente após wake IMU, antes do loop detectFall().
    // -------------------------------------------------------------------------
    void enableFallDetectionReports() {
        // Acelerômetro: normal mode, 100 Hz, ±8g
        writeReg(BMI_REG_CMD, BMI_CMD_ACC_NORMAL);
        delay(3);
        writeReg(BMI_REG_ACC_CONF,  BMI_ACC_CONF_NORMAL);   // 100Hz, bwp=normal
        writeReg(BMI_REG_ACC_RANGE, BMI_ACC_RANGE_8G);

        // Giroscópio: normal mode, 200 Hz, ±1000°/s
        writeReg(BMI_REG_CMD, BMI_CMD_GYR_NORMAL);
        delay(85);
        writeReg(BMI_REG_GYR_CONF,  BMI_GYR_CONF_NORMAL);
        writeReg(BMI_REG_GYR_RANGE, BMI_GYR_RANGE_1000);

        // Desabilitar interrupções durante a análise
        writeReg(BMI_REG_INT_EN_0, 0x00);
        writeReg(BMI_REG_INT_EN_1, 0x00);
        writeReg(BMI_REG_INT_EN_2, 0x00);

        delay(50);   // Aguardar estabilização dos filtros
        out.println("[IMU] BMI160 full-rate: 100Hz ±8g, 200Hz ±1000dps");
    }

    // -------------------------------------------------------------------------
    // enableWakeOnMotion() — equivalente ao WOM do MPU6500
    //
    // Estratégia:
    //   1. Gyro → suspend (economia de ~700 µA)
    //   2. Accel → Low Power Mode a 25 Hz (duty cycling, ~3 µA)
    //   3. Any-motion interrupt no INT1 (active LOW, non-latched)
    //
    // Consumo resultante: ~3–6 µA (BMI160) vs ~8 µA (MPU6500 WOM)
    //
    // Nota sobre o pino INT:
    //   O BMI160 não tem pino RST. O pino INT1 é active LOW aqui.
    //   No EXT0 do ESP32-S3, configurar wake em nível LOW — idêntico ao MPU6500.
    //   Limpar a interrupção: basta ler INT_STATUS_0 (0x1C).
    // -------------------------------------------------------------------------
    void enableWakeOnMotion() {
        // 1. Gyro → suspend
        writeReg(BMI_REG_CMD, BMI_CMD_GYR_SUSPEND);
        delay(5);

        // 2. Accel range e ODR (configurar antes de entrar em LP)
        writeReg(BMI_REG_ACC_CONF,  BMI_ACC_CONF_LP);    // acc_us=1, 25Hz, avg4
        writeReg(BMI_REG_ACC_RANGE, BMI_ACC_RANGE_8G);   // ±8g

        // 3. Configurar any-motion interrupt
        //    Duração: 2 amostras consecutivas acima do limiar
        writeReg(BMI_REG_INT_MOTION_0, BMI_ANYMOTION_DUR_2);
        writeReg(BMI_REG_INT_MOTION_1, BMI_ANYMOTION_THR);

        // 4. Configurar pino INT1: active LOW, push-pull, output habilitado
        writeReg(BMI_REG_INT_OUT_CTRL, BMI_INT1_ACTIVE_LOW);

        // 5. Non-latched: limpa sozinho ao ler INT_STATUS
        writeReg(BMI_REG_INT_LATCH, BMI_INT_LATCH_NONLATCH);

        // 6. Mapear any-motion para INT1
        writeReg(BMI_REG_INT_MAP_0, BMI_MAP0_ANYMOTION_INT1);
        writeReg(BMI_REG_INT_MAP_1, 0x00);
        writeReg(BMI_REG_INT_MAP_2, 0x00);

        // 7. Habilitar any-motion em X, Y, Z
        writeReg(BMI_REG_INT_EN_0, BMI_INT_EN0_ANYMOTION);
        writeReg(BMI_REG_INT_EN_1, 0x00);

        // 8. Acelerômetro → Low Power Mode
        writeReg(BMI_REG_CMD, BMI_CMD_ACC_LP);
        delay(5);

        // Limpar qualquer interrupção pendente
        readReg(BMI_REG_INT_STATUS_0);
        readReg(BMI_REG_INT_STATUS_1);

        out.printf("[IMU] BMI160 any-motion ativo (thr=%d ≈ %dmg, dur=2 amostras)\n",
                   BMI_ANYMOTION_THR, BMI_ANYMOTION_THR * 31);
    }

    // -------------------------------------------------------------------------
    // clearInterrupt() — limpar interrupção pendente antes do deep sleep
    // No BMI160 a leitura do INT_STATUS libera o pino automaticamente
    // (modo non-latched). Chamado em power.h antes de configurar o EXT0.
    // -------------------------------------------------------------------------
    void clearInterrupt() {
        readReg(BMI_REG_INT_STATUS_0);
        readReg(BMI_REG_INT_STATUS_1);
    }

    // -------------------------------------------------------------------------
    // detectFall() — algoritmo 4 fases idêntico ao MPU6500
    // -------------------------------------------------------------------------
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

            // Fase 1: Free-fall
            if (!ffDetected) {
                if (svm < FF_THRESH_G) {
                    if (tFFStart == 0) tFFStart = now;
                    if ((now - tFFStart) >= FF_MIN_MS) {
                        ffDetected = true; tFFEnd = now; phase = FREEFALL;
                        out.printf("[FALL] F1: Free-fall SVM=%.2fg %lums\n", svm, now - tFFStart);
                    }
                } else { tFFStart = 0; }
            }

            // Fase 2: Impacto
            if (ffDetected && !impactDetected) {
                if ((now - tFFEnd) > FF_TO_IMPACT_MAX_MS) {
                    out.println("[FALL] F2: Timeout sem impacto"); phase = NO_FALL; break;
                }
                if (svm > IMPACT_THRESH_G && angVel > IMPACT_ROT_MIN_DPS) {
                    impactDetected = true; phase = IMPACT;
                    out.printf("[FALL] F2: Impacto SVM=%.2fg w=%.1fdps\n", svm, angVel);
                }
            }

            // Fase 3: Inatividade
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

            // Fase 4: Mudança de orientação
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

    // ─── Leitura de sensores ──────────────────────────────────────────────────
    // BMI160 armazena dados em little-endian (LSB primeiro), ao contrário do
    // MPU6500 que usa big-endian. Lemos gyro e accel em burst separados.
    void readSensors(SensorData& d) {
        // Ler gyro (0x0C, 6 bytes)
        Wire.beginTransmission(BMI_ADDR);
        Wire.write(BMI_REG_GYR_X_L);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)BMI_ADDR, (uint8_t)6);
        if (Wire.available() < 6) { d = {}; return; }

        int16_t rgx = (int16_t)((Wire.read()) | (Wire.read() << 8));
        int16_t rgy = (int16_t)((Wire.read()) | (Wire.read() << 8));
        int16_t rgz = (int16_t)((Wire.read()) | (Wire.read() << 8));

        // Ler accel (0x12, 6 bytes)
        Wire.beginTransmission(BMI_ADDR);
        Wire.write(BMI_REG_ACC_X_L);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)BMI_ADDR, (uint8_t)6);
        if (Wire.available() < 6) { d = {}; return; }

        int16_t rax = (int16_t)((Wire.read()) | (Wire.read() << 8));
        int16_t ray = (int16_t)((Wire.read()) | (Wire.read() << 8));
        int16_t raz = (int16_t)((Wire.read()) | (Wire.read() << 8));

        d.ax = rax * BMI_ACCEL_SCALE;
        d.ay = ray * BMI_ACCEL_SCALE;
        d.az = raz * BMI_ACCEL_SCALE;
        d.gx = rgx * BMI_GYRO_SCALE;
        d.gy = rgy * BMI_GYRO_SCALE;
        d.gz = rgz * BMI_GYRO_SCALE;
        d.svm      = sqrtf(d.ax*d.ax + d.ay*d.ay + d.az*d.az);
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
        filtered.svm      = sqrtf(filtered.ax*filtered.ax + filtered.ay*filtered.ay + filtered.az*filtered.az);
        filtered.angVelMag = sqrtf(filtered.gx*filtered.gx + filtered.gy*filtered.gy + filtered.gz*filtered.gz);
    }

    void writeReg(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(BMI_ADDR);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission();
        delay(1);   // BMI160 requer ~1 ms entre comandos CMD
    }

    uint8_t readReg(uint8_t reg) {
        Wire.beginTransmission(BMI_ADDR);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)BMI_ADDR, (uint8_t)1);
        return Wire.available() ? Wire.read() : 0xFF;
    }
};

#endif // IMU_BMI160_H
