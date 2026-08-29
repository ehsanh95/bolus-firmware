#ifndef EVENT_AGGREGATOR_H
#define EVENT_AGGREGATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "event_processor.h"

/*
 * Bounded RAM history used before the persistent flash journal is introduced.
 * The aggregate counters continue to count even if the retained event-record
 * ring overwrites old detail records.
 */
#define EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY       32U
#define EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY       32U

/*
 * Matching tolerance is only scheduler/whole-second timestamp tolerance.
 * It is NOT a biological event-classification threshold.
 */
#define EVENT_AGGREGATOR_LOOKBACK_TOLERANCE_S        1U

typedef enum
{
    EVENT_AGGREGATOR_OK = 0,
    EVENT_AGGREGATOR_ERROR_PARAM,
    EVENT_AGGREGATOR_ERROR_CONFIG,
    EVENT_AGGREGATOR_ERROR_NOT_INITIALIZED,
    EVENT_AGGREGATOR_ERROR_TIME,
    EVENT_AGGREGATOR_ERROR_PROCESSOR
} event_aggregator_status_t;

typedef struct
{
    uint32_t timestamp_s;
    int32_t temperature_mdeg_c;
} event_aggregator_temperature_point_t;

/*
 * One physical observation supplied by SensorService/application logic.
 *
 * motion_duration_ms is expected to come from the longer BMA observation
 * window. rotation_candidate remains externally decided until field data gives
 * us a defensible MPU gyro/angle threshold.
 */
typedef struct
{
    uint32_t timestamp_s;

    bool motion_event;
    uint16_t motion_duration_ms;

    bool temperature_valid;
    int32_t temperature_mdeg_c;

    bool rotation_candidate;
    uint16_t peak_angular_velocity_dps;
    uint16_t total_orientation_change_cdeg;
} event_aggregator_observation_t;

/* Compact detailed record retained for the current transmission window. */
typedef struct
{
    uint32_t timestamp_s;
    event_processor_flags_t flags;

    uint16_t motion_duration_ms;
    bool inter_event_interval_valid;
    uint16_t inter_event_interval_s;

    bool temperature_valid;
    int32_t temperature_mdeg_c;

    uint16_t peak_angular_velocity_dps;
    uint16_t total_orientation_change_cdeg;
} event_aggregator_event_record_t;

/*
 * Fifteen-minute (or active uplink-period) summary consumed by telemetry later.
 * No etiologic diagnosis is encoded here; these are physical-event counts and
 * compact temporal statistics.
 */
typedef struct
{
    uint32_t window_start_s;
    uint32_t window_end_s;
    uint32_t window_period_s;

    bool temperature_valid;
    uint16_t temperature_sample_count;
    int32_t temperature_current_mdeg_c;
    int32_t temperature_min_mdeg_c;
    int32_t temperature_max_mdeg_c;
    int32_t max_negative_excursion_mdeg_c;

    uint16_t motion_event_count;
    uint16_t temp_drop_reference_count;
    uint16_t contraction_candidate_count;
    uint16_t contraction_periodicity_count;
    uint16_t hyperthermia_reference_count;
    uint16_t rotation_candidate_count;

    bool inter_event_interval_valid;
    uint16_t inter_event_interval_count;
    uint16_t inter_event_interval_min_s;
    uint16_t inter_event_interval_max_s;
    uint16_t inter_event_interval_mean_s;
    uint32_t inter_event_interval_variance_s2;

    event_processor_flags_t combined_flags;

    uint16_t retained_event_record_count;
    uint16_t overwritten_event_record_count;
} event_aggregator_window_summary_t;

typedef struct
{
    bool initialized;
    uint32_t window_period_s;
    uint32_t last_observation_timestamp_s;
    bool last_observation_timestamp_valid;

    bool last_motion_timestamp_valid;
    uint32_t last_motion_timestamp_s;

    event_aggregator_temperature_point_t
        temperature_history[EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY];
    uint8_t temperature_history_head;
    uint8_t temperature_history_count;

    event_aggregator_event_record_t
        event_records[EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY];
    uint8_t event_record_head;
    uint8_t event_record_count;

    uint64_t inter_event_interval_sum_s;
    uint64_t inter_event_interval_sum_sq_s2;

    event_aggregator_window_summary_t summary;
} event_aggregator_t;

/* Initialize one aggregator instance from active runtime configuration. */
event_aggregator_status_t EventAggregator_Init(
    event_aggregator_t *aggregator,
    const bolus_runtime_config_t *config,
    uint32_t window_start_s);

/*
 * Process one observation, build 5/10-minute temperature-reference features
 * when matching history exists, call EventProcessor, and update window stats.
 */
event_aggregator_status_t EventAggregator_ProcessObservation(
    event_aggregator_t *aggregator,
    const bolus_runtime_config_t *config,
    const event_aggregator_observation_t *observation,
    event_processor_features_t *features,
    event_processor_result_t *result);

/* True when the active radio/uplink window has elapsed. */
bool EventAggregator_IsWindowDue(
    const event_aggregator_t *aggregator,
    uint32_t now_s);

/* Copy the current aggregate summary without mutating state. */
event_aggregator_status_t EventAggregator_GetWindowSummary(
    const event_aggregator_t *aggregator,
    event_aggregator_window_summary_t *summary);

/*
 * Read retained event detail in chronological order, index 0 = oldest retained.
 * Aggregate counts remain valid even when the bounded detail ring overwrites.
 */
event_aggregator_status_t EventAggregator_GetEventRecord(
    const event_aggregator_t *aggregator,
    uint16_t index,
    event_aggregator_event_record_t *record);

/*
 * Start a fresh uplink window after a successful telemetry ACK.
 * Temperature history and the last motion timestamp are intentionally retained
 * so 5/10-minute thermal trajectories and inter-event timing cross boundaries.
 */
event_aggregator_status_t EventAggregator_ResetWindow(
    event_aggregator_t *aggregator,
    uint32_t new_window_start_s);

#endif /* EVENT_AGGREGATOR_H */
