#include "bench_led_diagnostics.h"

#include "fault_manager.h"

#include <stddef.h>
#include <string.h>

volatile bench_led_diag_t bench_led_diag = {0};

#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)

#define BENCH_LED_SENSOR_PULSE_MS       120UL
#define BENCH_LED_RF_PATTERN_MAX_MS    620UL
#define BENCH_LED_STATUS_REPEAT_MS    4000UL

/* All state is owned by the main loop: no HAL calls, delays, or timers in IRQs. */
static bench_led_radio_snapshot_t s_previous_radio;
static uint32_t s_previous_sensor_count;
static uint32_t s_sensor_pulse_start_ms;
static uint32_t s_rf_pattern_start_ms;
static uint32_t s_last_status_signal_ms;
static uint32_t s_sensor_fault_bits;
static uint32_t s_mcu_fault_bits;
static uint32_t s_rf_fault_bits;
static bench_rf_event_t s_rf_pattern;
static bench_rf_event_t s_pending_rf_event;
static bool s_sensor_pulse_active;
static bool s_rf_pattern_active;
static bool s_status_signal_initialized;
static uint8_t s_output_mask;

/* Priority: a drop or TX failure must not be hidden by a later retry. */
static uint8_t EventPriority(bench_rf_event_t event)
{
    switch (event)
    {
        case BENCH_RF_EVENT_TX_DROP:         return 5U;
        case BENCH_RF_EVENT_TX_FAILURE:      return 4U;
        case BENCH_RF_EVENT_TX_SUCCESS:
        case BENCH_RF_EVENT_RESPONSE_SUCCESS: return 3U;
        case BENCH_RF_EVENT_DUTY_CYCLE:
        case BENCH_RF_EVENT_MAC_BUSY:        return 2U;
        case BENCH_RF_EVENT_TX_REQUEST:      return 1U;
        default:                             return 0U;
    }
}

static void QueueRfEvent(bench_rf_event_t event)
{
    if (EventPriority(event) >= EventPriority(s_pending_rf_event))
    {
        s_pending_rf_event = event;
    }
    bench_led_diag.last_rf_event = event;
}

static void SetLedMask(uint8_t mask)
{
    if (mask == s_output_mask)
    {
        return;
    }

    BolusLed_Set(BOLUS_LED_SENSOR, (mask & 0x01U) != 0U);
    BolusLed_Set(BOLUS_LED_MCU, (mask & 0x02U) != 0U);
    BolusLed_Set(BOLUS_LED_RF, (mask & 0x04U) != 0U);
    s_output_mask = mask;
    bench_led_diag.visible_led_mask = mask;
}

static bool RfPatternOn(bench_rf_event_t event, uint32_t elapsed_ms)
{
    switch (event)
    {
        case BENCH_RF_EVENT_TX_REQUEST:
        case BENCH_RF_EVENT_QUEUE_WAIT:
            return elapsed_ms < 150UL;   /* One short pulse. */

        case BENCH_RF_EVENT_TX_SUCCESS:
        case BENCH_RF_EVENT_RESPONSE_SUCCESS:
            return (elapsed_ms < 120UL) ||
                   ((elapsed_ms >= 220UL) && (elapsed_ms < 340UL));

        case BENCH_RF_EVENT_DUTY_CYCLE:
            return elapsed_ms < 420UL;   /* One long pulse. */

        case BENCH_RF_EVENT_MAC_BUSY:
        case BENCH_RF_EVENT_NO_SESSION:
            return (elapsed_ms < 100UL) ||
                   ((elapsed_ms >= 180UL) && (elapsed_ms < 280UL));

        case BENCH_RF_EVENT_TX_FAILURE:
        case BENCH_RF_EVENT_TX_DROP:
            return (elapsed_ms < 100UL) ||
                   ((elapsed_ms >= 180UL) && (elapsed_ms < 280UL)) ||
                   ((elapsed_ms >= 360UL) && (elapsed_ms < 460UL));

        default:
            return false;
    }
}

void BenchLedDiagnostics_Init(void)
{
    bolus_fault_descriptor_t descriptor;

    memset((void *)&bench_led_diag, 0, sizeof(bench_led_diag));
    memset(&s_previous_radio, 0, sizeof(s_previous_radio));
    s_previous_sensor_count = 0U;
    s_sensor_pulse_start_ms = 0U;
    s_rf_pattern_start_ms = 0U;
    s_last_status_signal_ms = 0U;
    s_sensor_fault_bits = 0U;
    s_mcu_fault_bits = 0U;
    s_rf_fault_bits = 0U;
    s_rf_pattern = BENCH_RF_EVENT_NONE;
    s_pending_rf_event = BENCH_RF_EVENT_NONE;
    s_sensor_pulse_active = false;
    s_rf_pattern_active = false;
    s_status_signal_initialized = false;
    s_output_mask = 0xFFU;

    for (uint32_t id = 0U; id < (uint32_t)BOLUS_FAULT_COUNT; id++)
    {
        if (!FaultManager_GetDescriptor((bolus_fault_id_t)id, &descriptor))
        {
            continue;
        }
        switch (descriptor.domain)
        {
            case BOLUS_FAULT_DOMAIN_SENSOR:
            case BOLUS_FAULT_DOMAIN_BATTERY:
                if (id == (uint32_t)BOLUS_FAULT_BATTERY_CRITICAL)
                    s_mcu_fault_bits |= BOLUS_FAULT_BIT(id);
                else
                    s_sensor_fault_bits |= BOLUS_FAULT_BIT(id);
                break;
            case BOLUS_FAULT_DOMAIN_RF:
                s_rf_fault_bits |= BOLUS_FAULT_BIT(id);
                break;
            default:
                s_mcu_fault_bits |= BOLUS_FAULT_BIT(id);
                break;
        }
    }

    bench_led_diag.enabled = true;
    SetLedMask(0U);
}

void BenchLedDiagnostics_Process(
    uint32_t now_ms,
    bool radio_critical,
    uint32_t sensor_snapshot_count,
    const bench_led_radio_snapshot_t *radio)
{
    uint32_t active;
    uint8_t desired = 0U;

    if (radio == NULL)
        return;

    active = FaultManager_GetActiveMask();
    bench_led_diag.active_fault_mask = active;
    bench_led_diag.sensor_fault_mask = active & s_sensor_fault_bits;
    bench_led_diag.mcu_fault_mask = active & s_mcu_fault_bits;
    bench_led_diag.rf_fault_mask = active & s_rf_fault_bits;
    bench_led_diag.sensor_snapshot_count = sensor_snapshot_count;

    /* Cumulative counters are the primary diagnostic record (LEDs can be off
     * in STOP2 or suppressed during the Class A critical TX/RX interval). */
    bench_led_diag.tx_request_count = radio->tx_request_count;
    bench_led_diag.tx_success_count = radio->tx_success_count;
    bench_led_diag.tx_failure_count = radio->tx_failure_count;
    bench_led_diag.tx_drop_count = radio->tx_drop_count;
    bench_led_diag.duty_cycle_defer_count = radio->duty_cycle_defer_count;
    bench_led_diag.mac_busy_defer_count = radio->mac_busy_defer_count;
    bench_led_diag.response_tx_success_count = radio->response_tx_success_count;

    if (sensor_snapshot_count != s_previous_sensor_count)
    {
        s_previous_sensor_count = sensor_snapshot_count;
        s_sensor_pulse_active = true;
        s_sensor_pulse_start_ms = now_ms;
    }

    if (radio->tx_request_count != s_previous_radio.tx_request_count)
        QueueRfEvent(BENCH_RF_EVENT_TX_REQUEST);
    if (radio->duty_cycle_defer_count != s_previous_radio.duty_cycle_defer_count)
        QueueRfEvent(BENCH_RF_EVENT_DUTY_CYCLE);
    if (radio->mac_busy_defer_count != s_previous_radio.mac_busy_defer_count)
        QueueRfEvent(BENCH_RF_EVENT_MAC_BUSY);
    if (radio->tx_success_count != s_previous_radio.tx_success_count)
        QueueRfEvent(BENCH_RF_EVENT_TX_SUCCESS);
    if (radio->response_tx_success_count != s_previous_radio.response_tx_success_count)
        QueueRfEvent(BENCH_RF_EVENT_RESPONSE_SUCCESS);
    if (radio->tx_failure_count != s_previous_radio.tx_failure_count)
        QueueRfEvent(BENCH_RF_EVENT_TX_FAILURE);
    if (radio->tx_drop_count != s_previous_radio.tx_drop_count)
        QueueRfEvent(BENCH_RF_EVENT_TX_DROP);
    s_previous_radio = *radio;

    /* The MAC owns RX1/RX2 timing; never blink while radio-critical. Do not
     * hold the CPU awake to wait for a TX/RX event to finish. */
    if (radio_critical)
    {
        SetLedMask(0U);
        return;
    }

    if (!s_status_signal_initialized)
    {
        s_status_signal_initialized = true;
        s_last_status_signal_ms = now_ms;
    }

    if (!s_rf_pattern_active && (s_pending_rf_event != BENCH_RF_EVENT_NONE))
    {
        s_rf_pattern = s_pending_rf_event;
        s_pending_rf_event = BENCH_RF_EVENT_NONE;
        s_rf_pattern_start_ms = now_ms;
        s_rf_pattern_active = true;
        s_last_status_signal_ms = now_ms;
    }
    else if (!s_rf_pattern_active &&
             ((uint32_t)(now_ms - s_last_status_signal_ms) >= BENCH_LED_STATUS_REPEAT_MS))
    {
        /* Show persistent waiting once per normal scheduler wake, not by
         * adding a new RTC wakeup or changing the uplink deadline. */
        s_last_status_signal_ms = now_ms;
        if (!radio->credentials_provisioned || !radio->session_ready)
            s_rf_pattern = BENCH_RF_EVENT_NO_SESSION;
        else if ((radio->queue_count > 0U) && !radio->tx_in_flight)
            s_rf_pattern = BENCH_RF_EVENT_QUEUE_WAIT;
        else
            s_rf_pattern = BENCH_RF_EVENT_NONE;
        if (s_rf_pattern != BENCH_RF_EVENT_NONE)
        {
            s_rf_pattern_active = true;
            s_rf_pattern_start_ms = now_ms;
            bench_led_diag.last_rf_event = s_rf_pattern;
        }
    }

    if (s_rf_pattern_active &&
        ((uint32_t)(now_ms - s_rf_pattern_start_ms) >= BENCH_LED_RF_PATTERN_MAX_MS))
    {
        s_rf_pattern_active = false;
        s_rf_pattern = BENCH_RF_EVENT_NONE;
    }
    if (s_sensor_pulse_active &&
        ((uint32_t)(now_ms - s_sensor_pulse_start_ms) >= BENCH_LED_SENSOR_PULSE_MS))
        s_sensor_pulse_active = false;

    if (bench_led_diag.sensor_fault_mask != 0U || s_sensor_pulse_active)
        desired |= 0x01U;
    if (bench_led_diag.mcu_fault_mask != 0U)
        desired |= 0x02U;
    if (bench_led_diag.rf_fault_mask != 0U ||
        (s_rf_pattern_active &&
         RfPatternOn(s_rf_pattern, (uint32_t)(now_ms - s_rf_pattern_start_ms))))
        desired |= 0x04U;
    SetLedMask(desired);
}

bool BenchLedDiagnostics_IsPresenting(uint32_t now_ms)
{
    /* Only the optional bench patterns delay STOP2, by <=620 ms. The radio
     * critical check in main still takes precedence. */
    return ((s_sensor_pulse_active &&
             (uint32_t)(now_ms - s_sensor_pulse_start_ms) < BENCH_LED_SENSOR_PULSE_MS) ||
            (s_rf_pattern_active &&
             (uint32_t)(now_ms - s_rf_pattern_start_ms) < BENCH_LED_RF_PATTERN_MAX_MS));
}

void BenchLedDiagnostics_BeforeStop2(void)
{
    /* Force all pins low; persistent faults remain visible via the watch
     * struct and are redisplayed on the next normal wake. */
    SetLedMask(0U);
}

#else /* Release / explicit opt-out */

void BenchLedDiagnostics_Init(void) {}
void BenchLedDiagnostics_Process(uint32_t now_ms, bool radio_critical,
                                 uint32_t sensor_snapshot_count,
                                 const bench_led_radio_snapshot_t *radio)
{
    (void)now_ms;
    (void)radio_critical;
    (void)sensor_snapshot_count;
    (void)radio;
}
bool BenchLedDiagnostics_IsPresenting(uint32_t now_ms)
{
    (void)now_ms;
    return false;
}
void BenchLedDiagnostics_BeforeStop2(void) {}

#endif
