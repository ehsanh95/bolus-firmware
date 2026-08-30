#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Internal 15-minute telemetry snapshot.
 *
 * This is an application data model, not the on-air representation. A separate
 * codec explicitly quantizes and serializes it into the compact V1 payload.
 */
typedef struct
{
    uint16_t sequence;
    uint16_t config_version;

    bool temperature_valid;
    int32_t temperature_current_mdeg_c;
    int32_t temperature_min_mdeg_c;
    int32_t temperature_max_mdeg_c;
    int32_t max_negative_excursion_mdeg_c;

    bool motion_valid;
    uint16_t motion_event_count;
    uint16_t contraction_candidate_count;
    uint16_t rotation_candidate_count;

    bool inter_event_interval_valid;
    uint16_t inter_event_interval_mean_s;
    uint32_t inter_event_interval_variance_s2;

    uint16_t rms_dynamic_accel_mg;
    uint16_t peak_dynamic_accel_mg;

    uint32_t combined_event_flags;

    uint16_t battery_mv;
    uint8_t battery_percent;

    bool event_record_overflow;
    bool fault_present;
    bool health_degraded;
    bool health_critical;
} bolus_telemetry_summary_v1_t;

#endif /* TELEMETRY_DATA_H */
