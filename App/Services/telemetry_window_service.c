#include "telemetry_window_service.h"

#include <limits.h>
#include <string.h>

static void IncrementSaturatingU16(uint16_t *value)
{
    if ((value != NULL) && (*value < UINT16_MAX))
    {
        (*value)++;
    }
}

static uint16_t SaturateU16(uint64_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
}

static uint32_t SaturateU32(uint64_t value)
{
    return (value > UINT32_MAX) ? UINT32_MAX : (uint32_t)value;
}

static void ResetWindowData(
    telemetry_window_service_t *service,
    uint32_t window_start_ms)
{
    uint32_t uplink_period_ms = service->uplink_period_ms;
    uint16_t sequence_counter = service->sequence_counter;
    bool initialized = service->initialized;

    memset(service, 0, sizeof(*service));

    service->initialized = initialized;
    service->uplink_period_ms = uplink_period_ms;
    service->window_start_ms = window_start_ms;
    service->sequence_counter = sequence_counter;
}

telemetry_window_status_t TelemetryWindow_Init(
    telemetry_window_service_t *service,
    const bolus_runtime_config_t *config,
    uint32_t now_ms)
{
    if ((service == NULL) || (config == NULL))
    {
        return TELEMETRY_WINDOW_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return TELEMETRY_WINDOW_ERROR_CONFIG;
    }

    memset(service, 0, sizeof(*service));
    service->initialized = true;
    service->uplink_period_ms = config->radio.uplink_period_s * 1000UL;
    service->window_start_ms = now_ms;

    return TELEMETRY_WINDOW_OK;
}

bool TelemetryWindow_IsDue(
    const telemetry_window_service_t *service,
    uint32_t now_ms)
{
    if ((service == NULL) || (!service->initialized))
    {
        return false;
    }

    return ((now_ms - service->window_start_ms) >= service->uplink_period_ms);
}

void TelemetryWindow_RecordEpisodeAction(
    telemetry_window_service_t *service,
    const event_episode_action_t *action)
{
    if ((service == NULL) || (!service->initialized) || (action == NULL))
    {
        return;
    }

    if (action->episode_started)
    {
        IncrementSaturatingU16(&service->episode_count);
    }

    if (action->pulse_accepted)
    {
        IncrementSaturatingU16(&service->accepted_pulse_count);

        if (action->pulse_count > service->max_pulses_per_episode)
        {
            service->max_pulses_per_episode = action->pulse_count;
        }
    }

    if (action->pulse_suppressed_by_guard)
    {
        IncrementSaturatingU16(&service->suppressed_pulse_count);
    }

    if (action->inter_pulse_interval_valid)
    {
        uint64_t interval_ms = action->inter_pulse_interval_ms;

        IncrementSaturatingU16(&service->inter_pulse_interval_count);
        service->inter_pulse_interval_sum_ms += interval_ms;
        service->inter_pulse_interval_sum_sq_ms2 += interval_ms * interval_ms;
    }
}

void TelemetryWindow_RecordTemperature(
    telemetry_window_service_t *service,
    int32_t temperature_mdeg_c)
{
    int32_t excursion_from_prior_max;

    if ((service == NULL) || (!service->initialized))
    {
        return;
    }

    if (!service->temperature_valid)
    {
        service->temperature_valid = true;
        service->temperature_current_mdeg_c = temperature_mdeg_c;
        service->temperature_min_mdeg_c = temperature_mdeg_c;
        service->temperature_max_mdeg_c = temperature_mdeg_c;
        service->max_negative_excursion_mdeg_c = 0;
        return;
    }

    excursion_from_prior_max =
        temperature_mdeg_c - service->temperature_max_mdeg_c;

    if (excursion_from_prior_max < service->max_negative_excursion_mdeg_c)
    {
        service->max_negative_excursion_mdeg_c = excursion_from_prior_max;
    }

    if (temperature_mdeg_c < service->temperature_min_mdeg_c)
    {
        service->temperature_min_mdeg_c = temperature_mdeg_c;
    }

    if (temperature_mdeg_c > service->temperature_max_mdeg_c)
    {
        service->temperature_max_mdeg_c = temperature_mdeg_c;
    }

    service->temperature_current_mdeg_c = temperature_mdeg_c;
}

void TelemetryWindow_RecordMpuBurst(
    telemetry_window_service_t *service,
    const sensor_service_mpu_burst_features_t *features)
{
    if ((service == NULL) || (!service->initialized) ||
        (features == NULL) || (features->sample_count == 0U))
    {
        return;
    }

    IncrementSaturatingU16(&service->mpu_burst_count);
    service->mpu_rms_dynamic_accel_sum_mg +=
        features->rms_dynamic_accel_mg;
    service->mpu_rms_angular_velocity_sum_dps +=
        features->rms_angular_velocity_dps;
    service->mpu_total_angular_motion_cdeg +=
        features->total_angular_motion_cdeg;

    if (features->peak_dynamic_accel_mg > service->mpu_peak_dynamic_accel_mg)
    {
        service->mpu_peak_dynamic_accel_mg =
            features->peak_dynamic_accel_mg;
    }

    if (features->peak_angular_velocity_dps >
        service->mpu_peak_angular_velocity_dps)
    {
        service->mpu_peak_angular_velocity_dps =
            features->peak_angular_velocity_dps;
    }

    if (features->orientation_change_valid &&
        (features->orientation_change_cdeg >
         service->mpu_max_orientation_change_cdeg))
    {
        service->mpu_max_orientation_change_cdeg =
            features->orientation_change_cdeg;
    }
}

void TelemetryWindow_RecordBma456(
    telemetry_window_service_t *service,
    uint32_t step_count,
    int16_t accel_x_mg,
    int16_t accel_y_mg,
    int16_t accel_z_mg)
{
    if ((service == NULL) || (!service->initialized))
    {
        return;
    }

    service->bma456_valid = true;
    service->bma_step_count = step_count;
    service->bma_accel_x_mg = accel_x_mg;
    service->bma_accel_y_mg = accel_y_mg;
    service->bma_accel_z_mg = accel_z_mg;
}

telemetry_window_status_t TelemetryWindow_FreezeSummaryV2(
    telemetry_window_service_t *service,
    const bolus_runtime_config_t *config,
    uint32_t now_ms,
    uint16_t battery_mv,
    uint8_t battery_percent,
    bool fault_present,
    bool health_degraded,
    bool health_critical,
    bolus_telemetry_summary_v2_t *summary)
{
    uint64_t mean_interval_ms = 0U;
    uint64_t mean_interval_sq_ms2 = 0U;
    uint64_t variance_interval_ms2 = 0U;

    if ((service == NULL) || (config == NULL) || (summary == NULL))
    {
        return TELEMETRY_WINDOW_ERROR_PARAM;
    }

    if ((!service->initialized) || (!BolusRuntimeConfig_Validate(config)))
    {
        return TELEMETRY_WINDOW_ERROR_CONFIG;
    }

    if (!TelemetryWindow_IsDue(service, now_ms))
    {
        return TELEMETRY_WINDOW_NOT_DUE;
    }

    memset(summary, 0, sizeof(*summary));

    if (service->sequence_counter < UINT16_MAX)
    {
        service->sequence_counter++;
    }
    else
    {
        service->sequence_counter = 1U;
    }

    summary->sequence = service->sequence_counter;
    summary->config_version = config->version;

    summary->temperature_valid = service->temperature_valid;
    summary->temperature_current_mdeg_c =
        service->temperature_current_mdeg_c;
    summary->temperature_min_mdeg_c = service->temperature_min_mdeg_c;
    summary->temperature_max_mdeg_c = service->temperature_max_mdeg_c;
    summary->max_negative_excursion_mdeg_c =
        service->max_negative_excursion_mdeg_c;

    summary->motion_valid =
        ((service->accepted_pulse_count > 0U) || (service->episode_count > 0U));
    summary->episode_count = service->episode_count;
    summary->accepted_pulse_count = service->accepted_pulse_count;
    summary->suppressed_pulse_count = service->suppressed_pulse_count;
    summary->max_pulses_per_episode = service->max_pulses_per_episode;

    if (service->inter_pulse_interval_count > 0U)
    {
        mean_interval_ms =
            service->inter_pulse_interval_sum_ms /
            (uint64_t)service->inter_pulse_interval_count;
        mean_interval_sq_ms2 =
            service->inter_pulse_interval_sum_sq_ms2 /
            (uint64_t)service->inter_pulse_interval_count;
        variance_interval_ms2 =
            (mean_interval_sq_ms2 >= (mean_interval_ms * mean_interval_ms))
                ? (mean_interval_sq_ms2 - (mean_interval_ms * mean_interval_ms))
                : 0U;

        summary->inter_pulse_interval_valid = true;
        summary->inter_pulse_interval_mean_s = SaturateU16(
            (mean_interval_ms + 500ULL) / 1000ULL);
        summary->inter_pulse_interval_variance_s2 = SaturateU32(
            variance_interval_ms2 / 1000000ULL);
    }

    if (service->mpu_burst_count > 0U)
    {
        summary->mpu_valid = true;
        summary->mpu_burst_count = service->mpu_burst_count;
        summary->rms_dynamic_accel_mg = SaturateU16(
            service->mpu_rms_dynamic_accel_sum_mg /
            (uint64_t)service->mpu_burst_count);
        summary->peak_dynamic_accel_mg = service->mpu_peak_dynamic_accel_mg;
        summary->rms_angular_velocity_dps = SaturateU16(
            service->mpu_rms_angular_velocity_sum_dps /
            (uint64_t)service->mpu_burst_count);
        summary->peak_angular_velocity_dps =
            service->mpu_peak_angular_velocity_dps;
        summary->total_angular_motion_cdeg = SaturateU32(
            service->mpu_total_angular_motion_cdeg);
        summary->max_orientation_change_cdeg =
            service->mpu_max_orientation_change_cdeg;
    }

    summary->contraction_candidate_count =
        service->contraction_candidate_count;
    summary->rotation_candidate_count = service->rotation_candidate_count;
    summary->combined_event_flags = service->combined_event_flags;

    summary->battery_mv = battery_mv;
    summary->battery_percent = (battery_percent > 100U) ? 100U : battery_percent;
    summary->fault_present = fault_present;
    summary->health_degraded = health_degraded;
    summary->health_critical = health_critical;
    summary->staging_untested = true;

    ResetWindowData(service, now_ms);
    return TELEMETRY_WINDOW_OK;
}

telemetry_window_status_t TelemetryWindow_FreezeSummaryV2_1(
    telemetry_window_service_t *service,
    const bolus_runtime_config_t *config,
    uint32_t now_ms,
    uint16_t battery_mv,
    uint8_t battery_percent,
    bool fault_present,
    bool health_degraded,
    bool health_critical,
    bolus_telemetry_summary_v2_1_t *summary)
{
    telemetry_window_status_t status;
    uint32_t bma_step_count;
    int16_t bma_accel_x_mg;
    int16_t bma_accel_y_mg;
    int16_t bma_accel_z_mg;

    if ((service == NULL) || (config == NULL) || (summary == NULL))
    {
        return TELEMETRY_WINDOW_ERROR_PARAM;
    }

    memset(summary, 0, sizeof(*summary));

    /* Save the BMA snapshot before the legacy freeze rolls the accumulator. */
    bma_step_count = service->bma_step_count;
    bma_accel_x_mg = service->bma_accel_x_mg;
    bma_accel_y_mg = service->bma_accel_y_mg;
    bma_accel_z_mg = service->bma_accel_z_mg;

    status = TelemetryWindow_FreezeSummaryV2(
        service,
        config,
        now_ms,
        battery_mv,
        battery_percent,
        fault_present,
        health_degraded,
        health_critical,
        &summary->v2);

    if (status != TELEMETRY_WINDOW_OK)
    {
        return status;
    }

    summary->bma_step_count = bma_step_count;
    summary->bma_accel_x_mg = bma_accel_x_mg;
    summary->bma_accel_y_mg = bma_accel_y_mg;
    summary->bma_accel_z_mg = bma_accel_z_mg;

    return TELEMETRY_WINDOW_OK;
}
