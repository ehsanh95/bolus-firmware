#ifndef BENCH_LED_DIAGNOSTICS_H
#define BENCH_LED_DIAGNOSTICS_H

#include <stdbool.h>
#include <stdint.h>

#include "bolus_led.h"

/*
 * Optional, non-blocking bench-only LED diagnostics.
 *
 * DEBUG builds enable this by default; Release builds compile every LED GPIO
 * write out via bolus_led.c. Define BOLUS_BENCH_LED_DIAGNOSTICS=0 explicitly
 * for power measurements made with a Debug build.
 *
 * This module never toggles LEDs while a LoRaWAN TX/RX window is critical.
 * Completed event counters remain visible in the debugger even during STOP2.
 */
typedef enum
{
    BENCH_RF_EVENT_NONE = 0,
    BENCH_RF_EVENT_TX_REQUEST,
    BENCH_RF_EVENT_TX_SUCCESS,
    BENCH_RF_EVENT_TX_FAILURE,
    BENCH_RF_EVENT_TX_DROP,
    BENCH_RF_EVENT_DUTY_CYCLE,
    BENCH_RF_EVENT_MAC_BUSY,
    BENCH_RF_EVENT_RESPONSE_SUCCESS,
    BENCH_RF_EVENT_QUEUE_WAIT,
    BENCH_RF_EVENT_NO_SESSION
} bench_rf_event_t;

/* Copy only scalar counters; never read or modify LoRaMAC internals here. */
typedef struct
{
    bool credentials_provisioned;
    bool session_ready;
    bool tx_in_flight;
    uint8_t queue_count;
    uint32_t tx_request_count;
    uint32_t tx_success_count;
    uint32_t tx_failure_count;
    uint32_t tx_drop_count;
    uint32_t duty_cycle_defer_count;
    uint32_t mac_busy_defer_count;
    uint32_t response_tx_success_count;
} bench_led_radio_snapshot_t;

typedef struct
{
    bool enabled;
    uint32_t sensor_snapshot_count;
    uint32_t tx_request_count;
    uint32_t tx_success_count;
    uint32_t tx_failure_count;
    uint32_t tx_drop_count;
    uint32_t duty_cycle_defer_count;
    uint32_t mac_busy_defer_count;
    uint32_t response_tx_success_count;
    uint32_t active_fault_mask;
    uint32_t sensor_fault_mask;
    uint32_t mcu_fault_mask;
    uint32_t rf_fault_mask;
    bench_rf_event_t last_rf_event;
    uint8_t visible_led_mask; /* Bit 0 = LD1, bit 1 = LD2, bit 2 = LD3. */
} bench_led_diag_t;

extern volatile bench_led_diag_t bench_led_diag;

void BenchLedDiagnostics_Init(void);
void BenchLedDiagnostics_Process(
    uint32_t now_ms,
    bool radio_critical,
    uint32_t sensor_snapshot_count,
    const bench_led_radio_snapshot_t *radio);
bool BenchLedDiagnostics_IsPresenting(uint32_t now_ms);
void BenchLedDiagnostics_BeforeStop2(void);

#endif /* BENCH_LED_DIAGNOSTICS_H */
