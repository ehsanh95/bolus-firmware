#ifndef EVENT_PROCESSOR_H
#define EVENT_PROCESSOR_H

#include <stdbool.h>
#include <stdint.h>

#include "../Config/bolus_runtime_config.h"

/*
 * Physical/event flags intentionally precede behavioral diagnoses.
 * Multiple flags may be present for the same observation.
 */
typedef uint32_t event_processor_flags_t;

#define EVENT_PROCESSOR_FLAG_GENERAL_MOTION          (1UL << 0)
#define EVENT_PROCESSOR_FLAG_TEMP_DROP_REFERENCE     (1UL << 1)
#define EVENT_PROCESSOR_FLAG_CONTRACTION_CANDIDATE   (1UL << 2)
#define EVENT_PROCESSOR_FLAG_CONTRACTION_PERIODICITY (1UL << 3)
#define EVENT_PROCESSOR_FLAG_HYPERTHERMIA_REFERENCE  (1UL << 4)
#define EVENT_PROCESSOR_FLAG_ROTATION_CANDIDATE      (1UL << 5)

typedef enum
{
    EVENT_PROCESSOR_OK = 0,
    EVENT_PROCESSOR_ERROR_PARAM,
    EVENT_PROCESSOR_ERROR_CONFIG
} event_processor_status_t;

/*
 * One feature snapshot supplied by the acquisition/temporal-aggregation layer.
 *
 * Temperature deltas use: current - earlier temperature.
 * Therefore a cooling event is negative, e.g. -700 mdegC = -0.7 C.
 *
 * rotation_candidate is deliberately supplied by the caller because the
 * literature review found no validated universal intraruminal gyro threshold.
 */
typedef struct
{
    uint32_t timestamp_s;

    bool motion_event;
    uint16_t motion_duration_ms;

    bool inter_event_interval_valid;
    uint16_t inter_event_interval_s;

    bool temperature_valid;
    int32_t temperature_mdeg_c;

    bool temperature_delta_5min_valid;
    int32_t temperature_delta_5min_mdeg_c;

    bool temperature_delta_10min_valid;
    int32_t temperature_delta_10min_mdeg_c;

    bool rotation_candidate;
    uint16_t peak_angular_velocity_dps;
    uint16_t total_orientation_change_cdeg;
} event_processor_features_t;

typedef struct
{
    event_processor_flags_t flags;
    bolus_event_rule_source_t rule_source;

    bool contraction_duration_match;
    bool contraction_interval_match;
    bool drinking_5min_reference_match;
    bool drinking_10min_reference_match;

    uint32_t timestamp_s;
} event_processor_result_t;

/*
 * Evaluate already-computed features using active RuntimeConfig.
 *
 * When rule_source == BOLUS_EVENT_RULES_REFERENCE_BENCHMARK, resulting flags
 * are research/reference matches only and must not be promoted to definitive
 * drinking/disease labels without Bolus field calibration.
 */
event_processor_status_t EventProcessor_Evaluate(
    const bolus_runtime_config_t *config,
    const event_processor_features_t *features,
    event_processor_result_t *result);

#endif /* EVENT_PROCESSOR_H */
