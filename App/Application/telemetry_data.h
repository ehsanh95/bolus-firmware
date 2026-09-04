#ifndef TELEMETRY_DATA_H
#define TELEMETRY_DATA_H

#include <stdbool.h>
#include <stdint.h>

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
    uint16_t episode_count;
    uint16_t accepted_pulse_count;
    uint16_t suppressed_pulse_count;
    uint16_t max_pulses_per_episode;

    bool inter_pulse_interval_valid;
    uint16_t inter_pulse_interval_mean_s;
    uint32_t inter_pulse_interval_variance_s2;

    bool mpu_valid;
    uint16_t mpu_burst_count;
    uint16_t rms_dynamic_accel_mg;
    uint16_t peak_dynamic_accel_mg;
    uint16_t rms_angular_velocity_dps;
    uint16_t peak_angular_velocity_dps;
    uint32_t total_angular_motion_cdeg;
    uint16_t max_orientation_change_cdeg;

    uint16_t contraction_candidate_count;
    uint16_t rotation_candidate_count;
    uint32_t combined_event_flags;

    uint16_t battery_mv;
    uint8_t battery_percent;

    bool fault_present;
    bool health_degraded;
    bool health_critical;
    bool staging_untested;

    /* BMA456 native telemetry. Do not populate from MPU6050. */
    bool bma456_valid;
    uint32_t bma_step_count;
    int16_t bma_accel_x_mg;
    int16_t bma_accel_y_mg;
    int16_t bma_accel_z_mg;
} bolus_telemetry_summary_v2_t;

#endif /* TELEMETRY_DATA_H */
