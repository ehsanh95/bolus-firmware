#ifndef TELEMETRY_WINDOW_SERVICE_H
#define TELEMETRY_WINDOW_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "event_episode_service.h"
#include "sensor_service.h"
#include "../Application/telemetry_data.h"
#include "../Config/bolus_runtime_config.h"

typedef enum
{
    TELEMETRY_WINDOW_OK = 0,
    TELEMETRY_WINDOW_ERROR_PARAM,
    TELEMETRY_WINDOW_ERROR_CONFIG,
    TELEMETRY_WINDOW_NOT_DUE
} telemetry_window_status_t;

typedef struct
{
    bool initialized;
    uint32_t uplink_period_ms;
    uint32_t window_start_ms;
    uint16_t sequence_counter;

    uint16_t episode_count;
    uint16_t accepted_pulse_count;
    uint16_t suppressed_pulse_count;
    uint16_t max_pulses_per_episode;

    uint16_t inter_pulse_interval_count;
    uint64_t inter_pulse_interval_sum_ms;
    uint64_t inter_pulse_interval_sum_sq_ms2;

    bool temperature_valid;
    int32_t temperature_current_mdeg_c;
    int32_t temperature_min_mdeg_c;
    int32_t temperature_max_mdeg_c;
    int32_t max_negative_excursion_mdeg_c;

    uint16_t mpu_burst_count;
    uint64_t mpu_rms_dynamic_accel_sum_mg;
    uint16_t mpu_peak_dynamic_accel_mg;
    uint64_t mpu_rms_angular_velocity_sum_dps;
    uint16_t mpu_peak_angular_velocity_dps;
    uint64_t mpu_total_angular_motion_cdeg;
    uint16_t mpu_max_orientation_change_cdeg;

    /* Native BMA456 telemetry cache. Never sourced from MPU6050. */
    bool bma456_valid;
    uint32_t bma_step_count;
    int16_t bma_accel_x_mg;
    int16_t bma_accel_y_mg;
    int16_t bma_accel_z_mg;

    uint16_t contraction_candidate_count;
    uint16_t rotation_candidate_count;
    uint32_t combined_event_flags;
} telemetry_window_service_t;

telemetry_window_status_t TelemetryWindow_Init(
    telemetry_window_service_t *service,
    const bolus_runtime_config_t *config,
    uint32_t now_ms);

bool TelemetryWindow_IsDue(
    const telemetry_window_service_t *service,
    uint32_t now_ms);

void TelemetryWindow_RecordEpisodeAction(
    telemetry_window_service_t *service,
    const event_episode_action_t *action);

void TelemetryWindow_RecordTemperature(
    telemetry_window_service_t *service,
    int32_t temperature_mdeg_c);

void TelemetryWindow_RecordMpuBurst(
    telemetry_window_service_t *service,
    const sensor_service_mpu_burst_features_t *features);

void TelemetryWindow_RecordBma456(
    telemetry_window_service_t *service,
    uint32_t step_count,
    int16_t accel_x_mg,
    int16_t accel_y_mg,
    int16_t accel_z_mg);

telemetry_window_status_t TelemetryWindow_FreezeSummaryV2(
    telemetry_window_service_t *service,
    const bolus_runtime_config_t *config,
    uint32_t now_ms,
    uint16_t battery_mv,
    uint8_t battery_percent,
    bool fault_present,
    bool health_degraded,
    bool health_critical,
    bolus_telemetry_summary_v2_t *summary);

#endif /* TELEMETRY_WINDOW_SERVICE_H */
