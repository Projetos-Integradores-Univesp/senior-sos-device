#ifndef ULP_BLINK_H
#define ULP_BLINK_H

// ============================================================================
// ULP-FSM Alert LED Blink — ElderGuard XIAO ESP32S3
// Compatível com arduino-esp32 3.x / ESP-IDF 5.x
// ============================================================================
//
// CORREÇÃO EM RELAÇÃO À VERSÃO ANTERIOR:
//   No IDF 4.x existia RTC_GPIO_OUT_REG / RTC_GPIO_OUT_DATA_S.
//   No IDF 5.x (arduino-esp32 3.x) esses nomes foram substituídos por:
//     RTC_GPIO_OUT_W1TS_REG  + RTC_GPIO_OUT_DATA_W1TS_S  → seta bits (HIGH)
//     RTC_GPIO_OUT_W1TC_REG  + RTC_GPIO_OUT_DATA_W1TC_S  → limpa bits (LOW)
//   A abordagem W1TS/W1TC é mais segura porque é atômica (não precisa de
//   read-modify-write). Ambas as constantes são definidas em soc/rtc_io_reg.h.
//
// FUNÇÃO:
//   Pisca o LED de alerta (GPIO2, ativo em LOW) enquanto o ESP32 está em
//   deep sleep, usando o coprocessador ULP-FSM. O núcleo principal fica
//   completamente desligado durante o pisca — consumo total: ~95 µA.
//
// PARÂMETROS DE PISCA:
//   ON  = 100 ms  →  LED aceso   (duty cycle = 10%)
//   OFF = 900 ms  →  LED apagado
//   Frequência: ~1 Hz
//
// TIMING (RTC_SLOW_CLK = 136 kHz RC interno, ±5%):
//   ON  → I_DELAY(13600)           = 13600/136000 = 100,0 ms
//   OFF → I_DELAY(65535)           = 481,9 ms
//       + I_DELAY(56865)           = 418,1 ms  → total = 900,0 ms
//
// PROGRAMA ULP (6 instruções = 24 bytes, bem dentro dos 512 reservados):
//   I_WR_REG(W1TC, bit, bit, 1)  → GPIO2 LOW  (LED aceso)
//   I_DELAY(13600)
//   I_WR_REG(W1TS, bit, bit, 1)  → GPIO2 HIGH (LED apagado)
//   I_DELAY(65535)
//   I_DELAY(56865)
//   I_HALT()
//
// PRÉ-REQUISITOS:
//   Arquivo sdkconfig.defaults na raiz do projeto com:
//     CONFIG_ULP_COPROC_ENABLED=y
//     CONFIG_ULP_COPROC_TYPE_FSM=y
//     CONFIG_ULP_COPROC_RESERVE_MEM=512
//
// USO:
//   UlpBlink::start()  — chamar imediatamente antes de esp_deep_sleep_start()
//   UlpBlink::stop()   — chamar em alertLedOff() para parar pisca e hold DC
// ============================================================================

#include <Arduino.h>
#include "driver/rtc_io.h"
#include "soc/rtc_io_reg.h"    // RTC_GPIO_OUT_W1TS_REG, RTC_GPIO_OUT_W1TC_REG
                                // RTC_GPIO_OUT_DATA_W1TS_S, RTC_GPIO_OUT_DATA_W1TC_S
#include "soc/soc_ulp.h"       // I_WR_REG, I_DELAY, I_HALT, ulp_insn_t (IDF 5.x)
#include "esp32s3/ulp.h"       // ulp_process_macros_and_load, ulp_run (ESP32-S3 específico)
#include "esp_sleep.h"
#include "ulp.h"               // ulp_set_wakeup_period, ulp_timer_stop
#include "config.h"
#include "espnow_mirror.h"

// ============================================================================
// Timing
// ============================================================================
#define ULP_RTC_CLK_HZ   136000UL

#define ULP_ON_CYCLES    ((uint32_t)(100UL * ULP_RTC_CLK_HZ / 1000UL))   // 13 600
#define ULP_OFF_CYCLES_1  65535U
#define ULP_OFF_CYCLES_2  ((uint32_t)(900UL * ULP_RTC_CLK_HZ / 1000UL) - ULP_OFF_CYCLES_1) // 56 865

// Offset de início do programa ULP na RTC_SLOW_MEM (em palavras de 32 bits)
#define ULP_PROG_OFFSET   4U

// ============================================================================
// Mapeamento do pino no barramento RTC para IDF 5.x
//
// Para o ESP32-S3 (IDF 5.x / arduino-esp32 3.x):
//   RTC_GPIO_OUT_DATA_W1TS_S e RTC_GPIO_OUT_DATA_W1TC_S são definidos em
//   soc/rtc_io_reg.h. Para o GPIO N (que é também RTCIO N no S3):
//
//     bit W1TS = RTC_GPIO_OUT_DATA_W1TS_S + N
//     bit W1TC = RTC_GPIO_OUT_DATA_W1TC_S + N
//
//   Neste projeto ALERT_LED_PIN = GPIO2, portanto:
//     bit W1TS = RTC_GPIO_OUT_DATA_W1TS_S + 2
//     bit W1TC = RTC_GPIO_OUT_DATA_W1TC_S + 2
//
// LED é active LOW:
//   ACENDER  → W1TC (Clear → GPIO LOW)
//   APAGAR   → W1TS (Set   → GPIO HIGH)
// ============================================================================
#define ALERT_LED_W1TS_BIT  ((uint32_t)(RTC_GPIO_OUT_DATA_W1TS_S + ALERT_LED_PIN))
#define ALERT_LED_W1TC_BIT  ((uint32_t)(RTC_GPIO_OUT_DATA_W1TC_S + ALERT_LED_PIN))

class UlpBlink {
public:

    // -----------------------------------------------------------------------
    // start() — carrega e inicia o programa ULP-FSM
    //
    // Pré-condição: GPIO2 configurado como RTC output (feito em alertLedOn()).
    // Chamar imediatamente antes de esp_deep_sleep_start().
    // -----------------------------------------------------------------------
    static void start() {
        // Re-confirmar pino como RTC output sem hold
        // (alertLedOn() já fez, mas é seguro repetir)
        rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
        // Não ativar hold aqui — o ULP controla o pino diretamente

        // -----------------------------------------------------------------------
        // Programa ULP-FSM — IDF 5.x / arduino-esp32 3.x
        //
        // I_WR_REG(reg, low_bit, high_bit, value):
        //   Escreve 'value' nos bits [high_bit:low_bit] do registrador RTC.
        //   Para W1TS/W1TC, escrever 1 no bit correspondente seta/limpa o GPIO.
        //   low_bit == high_bit (operação em 1 bit).
        //
        // Nota: I_WR_REG suporta bit_width máximo de 8 bits e o campo deve
        // caber em byte alinhado. W1TS/W1TC têm os bits dos GPIO a partir
        // de RTC_GPIO_OUT_DATA_W1TS_S (= 10 no ESP32-S3), então GPIO2 fica
        // no bit 12 — dentro do byte [15:8] do registrador → válido.
        // -----------------------------------------------------------------------
        const ulp_insn_t ulp_prog[] = {

            // ── FASE ON: acender LED (GPIO2 = LOW via W1TC) ──
            // W1TC: escrever 1 no bit do GPIO limpa o output (LOW = LED aceso)
            I_WR_REG(RTC_GPIO_OUT_W1TC_REG,
                     ALERT_LED_W1TC_BIT, ALERT_LED_W1TC_BIT, 1),
            I_DELAY(ULP_ON_CYCLES),                    // 100 ms

            // ── FASE OFF: apagar LED (GPIO2 = HIGH via W1TS) ──
            // W1TS: escrever 1 no bit do GPIO seta o output (HIGH = LED apagado)
            I_WR_REG(RTC_GPIO_OUT_W1TS_REG,
                     ALERT_LED_W1TS_BIT, ALERT_LED_W1TS_BIT, 1),
            I_DELAY(ULP_OFF_CYCLES_1),                 // 481,9 ms
            I_DELAY(ULP_OFF_CYCLES_2),                 // 418,1 ms → total 900 ms

            // ── HALT: timer rearma o ULP após wakeup_period ──
            I_HALT(),
        };

        // Período mínimo (1 ms) — apenas para rearme do timer após HALT.
        // O timing real do pisca é completamente definido pelos I_DELAY acima.
        ulp_set_wakeup_period(0, 1000UL);

        // Carregar e iniciar o programa ULP
        size_t n = sizeof(ulp_prog) / sizeof(ulp_insn_t);
        esp_err_t err = ulp_process_macros_and_load(ULP_PROG_OFFSET, ulp_prog, &n);
        if (err != ESP_OK) {
            out.printf("[ULP] Falha load (err=%d) — fallback LED estático\n", err);
            _fallbackStaticOn();
            return;
        }

        err = ulp_run(ULP_PROG_OFFSET);
        if (err != ESP_OK) {
            out.printf("[ULP] Falha run (err=%d) — fallback LED estático\n", err);
            _fallbackStaticOn();
            return;
        }

        out.printf("[ULP] Pisca OK: %u ms ON / %u ms OFF (10%% duty, ~1 Hz)\n",
                   (unsigned)(ULP_ON_CYCLES * 1000UL / ULP_RTC_CLK_HZ),
                   (unsigned)((ULP_OFF_CYCLES_1 + ULP_OFF_CYCLES_2) * 1000UL / ULP_RTC_CLK_HZ));
    }

    // -----------------------------------------------------------------------
    // stop() — para o ULP e configura LED apagado com hold DC
    // -----------------------------------------------------------------------
    static void stop() {
        ulp_timer_stop();

        rtc_gpio_hold_dis((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_init((gpio_num_t)ALERT_LED_PIN);
        rtc_gpio_set_direction((gpio_num_t)ALERT_LED_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 1);  // HIGH = apagado
        rtc_gpio_hold_en((gpio_num_t)ALERT_LED_PIN);

        out.println("[ULP] Parado — LED apagado (hold DC HIGH)");
    }

private:
    static void _fallbackStaticOn() {
        rtc_gpio_set_level((gpio_num_t)ALERT_LED_PIN, 0);  // LOW = aceso
        rtc_gpio_hold_en((gpio_num_t)ALERT_LED_PIN);
        out.println("[ULP] Fallback: LED aceso estático (hold DC LOW)");
    }
};

#endif // ULP_BLINK_H
