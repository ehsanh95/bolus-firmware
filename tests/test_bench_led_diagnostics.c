#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bench_led_diagnostics.h"
#include "fault_manager.h"

#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)
/* Host GPIO shim: no real STM32 HAL or hardware dependency. */
static bool s_leds[3];
static uint32_t s_led_writes;

void BolusLed_Init(void) { memset(s_leds, 0, sizeof(s_leds)); }
void BolusLed_On(bolus_led_t led) { s_leds[led] = true; s_led_writes++; }
void BolusLed_Off(bolus_led_t led) { s_leds[led] = false; s_led_writes++; }
void BolusLed_Set(bolus_led_t led, bool value)
{
    if (value) BolusLed_On(led);
    else BolusLed_Off(led);
}
void BolusLed_AllOff(void)
{
    for (unsigned id = 0; id < 3U; id++) BolusLed_Off((bolus_led_t)id);
}

static void bench_patterns(void)
{
    bench_led_radio_snapshot_t radio = {0};

    FaultManager_Init();
    BenchLedDiagnostics_Init();
    assert(bench_led_diag.enabled);
    assert(!s_leds[0] && !s_leds[1] && !s_leds[2]);
    radio.credentials_provisioned = true;
    radio.session_ready = true;
    BenchLedDiagnostics_Process(0U, false, 0U, &radio);

    /* A successful telemetry freeze is ONE LD1 pulse, not proof of TX. */
    BenchLedDiagnostics_Process(10U, false, 1U, &radio);
    assert(s_leds[0] && BenchLedDiagnostics_IsPresenting(20U));
    BenchLedDiagnostics_Process(131U, false, 1U, &radio);
    assert(!s_leds[0] && !BenchLedDiagnostics_IsPresenting(131U));

    /* Clearing one of two faults sharing LD1 must not extinguish the other. */
    FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
    FaultManager_Raise(BOLUS_FAULT_BATTERY_LOW);
    assert(FaultManager_ClearFault(BOLUS_FAULT_TMP117_COMM));
    assert(s_leds[0]);
    BenchLedDiagnostics_Process(200U, false, 1U, &radio);
    assert(bench_led_diag.sensor_fault_mask != 0U && s_leds[0]);
    assert(fault_manager_diag.raise_call_count == 2U);
    assert(fault_manager_diag.clear_count == 1U);
    assert(fault_manager_diag.history_mask & BOLUS_FAULT_BIT(BOLUS_FAULT_TMP117_COMM));
    assert(FaultManager_ClearFault(BOLUS_FAULT_BATTERY_LOW));
    BenchLedDiagnostics_Process(210U, false, 1U, &radio);
    assert(!s_leds[0]);

    /* TX/RX critical state suppresses GPIO changes. Success is shown after
     * the critical interval by two short LD3 pulses. */
    radio.tx_request_count = 1U;
    BenchLedDiagnostics_Process(300U, true, 1U, &radio);
    assert(!s_leds[0] && !s_leds[1] && !s_leds[2]);
    radio.tx_success_count = 1U;
    BenchLedDiagnostics_Process(390U, true, 1U, &radio);
    assert(!s_leds[2]);
    BenchLedDiagnostics_Process(500U, false, 1U, &radio);
    assert(s_leds[2] && bench_led_diag.last_rf_event == BENCH_RF_EVENT_TX_SUCCESS);
    BenchLedDiagnostics_Process(650U, false, 1U, &radio);
    assert(!s_leds[2]);
    BenchLedDiagnostics_Process(750U, false, 1U, &radio);
    assert(s_leds[2]);
    BenchLedDiagnostics_Process(900U, false, 1U, &radio);
    assert(!s_leds[2]);
    assert(BenchLedDiagnostics_IsPresenting(1100U));
    BenchLedDiagnostics_Process(1130U, false, 1U, &radio);
    assert(!BenchLedDiagnostics_IsPresenting(1130U));

    radio.duty_cycle_defer_count = 1U;
    BenchLedDiagnostics_Process(2000U, false, 1U, &radio);
    assert(s_leds[2] && bench_led_diag.last_rf_event == BENCH_RF_EVENT_DUTY_CYCLE);
    BenchLedDiagnostics_Process(2430U, false, 1U, &radio);
    assert(!s_leds[2]);
    BenchLedDiagnostics_Process(2630U, false, 1U, &radio);

    radio.tx_drop_count = 1U;
    BenchLedDiagnostics_Process(3000U, false, 1U, &radio);
    assert(s_leds[2]);
    BenchLedDiagnostics_Process(3200U, false, 1U, &radio);
    assert(s_leds[2]);
    BenchLedDiagnostics_Process(3390U, false, 1U, &radio);
    assert(s_leds[2]);
    BenchLedDiagnostics_Process(3630U, false, 1U, &radio);
    assert(!s_leds[2]);

    FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
    BenchLedDiagnostics_Process(4000U, false, 1U, &radio);
    assert(s_leds[1] && bench_led_diag.mcu_fault_mask != 0U);
    assert(FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID));
    BenchLedDiagnostics_Process(4010U, false, 1U, &radio);
    assert(!s_leds[1]);

    FaultManager_Raise(BOLUS_FAULT_POWER_CONTROL);
    assert(!FaultManager_ClearFault(BOLUS_FAULT_POWER_CONTROL));
    BenchLedDiagnostics_Process(4200U, false, 1U, &radio);
    assert(s_leds[1]);
    FaultManager_ClearAll();
    BenchLedDiagnostics_Process(4220U, false, 1U, &radio);
    assert(!s_leds[1]);
    assert(fault_manager_diag.history_mask != 0U);
    assert(s_led_writes > 0U);

    /* A pending status pattern is driven only by regular scheduler calls. */
    radio.credentials_provisioned = false;
    radio.session_ready = false;
    BenchLedDiagnostics_Process(7100U, false, 1U, &radio);
    assert(bench_led_diag.last_rf_event == BENCH_RF_EVENT_NO_SESSION);
    assert(s_leds[2]);
    BenchLedDiagnostics_Process(7200U, false, 1U, &radio);
    assert(!s_leds[2]);
    BenchLedDiagnostics_BeforeStop2();
    assert(!s_leds[0] && !s_leds[1] && !s_leds[2]);
    puts("PASS: bench LED fault mapping, RF patterns, STOP2 and sticky counters");
}
#else
static void release_leds(void)
{
    /* Link the actual bolus_led.c under -D...=0, with no main.h/HAL. */
    bench_led_radio_snapshot_t radio = {0};
    FaultManager_Init();
    BenchLedDiagnostics_Init();
    assert(!bench_led_diag.enabled);
    FaultManager_Raise(BOLUS_FAULT_RF_COMM);
    BenchLedDiagnostics_Process(1000U, false, 1U, &radio);
    assert(!BenchLedDiagnostics_IsPresenting(1000U));
    BenchLedDiagnostics_BeforeStop2();
    puts("PASS: release build compiles with LED GPIO fully disabled");
}
#endif

int main(void)
{
#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)
    bench_patterns();
#else
    release_leds();
#endif
    return 0;
}
