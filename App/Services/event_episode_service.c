#include "event_episode_service.h"

#include <limits.h>
#include <string.h>

static bool TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0);
}

static void ClearAction(event_episode_action_t *action)
{
    memset(action, 0, sizeof(*action));
    action->temperature_source = EVENT_EPISODE_TEMP_SOURCE_NONE;
}

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

static void BuildSummary(
    const event_episode_service_t *service,
    event_episode_summary_t *summary)
{
    memset(summary, 0, sizeof(*summary));

    summary->episode_sequence = service->active_sequence;
    summary->start_time_ms = service->start_time_ms;
    summary->last_pulse_time_ms = service->last_pulse_time_ms;
    summary->close_deadline_ms = service->close_deadline_ms;
    summary->active_span_ms = service->last_pulse_time_ms - service->start_time_ms;

    summary->pulse_count = service->pulse_count;
    summary->suppressed_retrigger_count = service->suppressed_retrigger_count;

    summary->temperature_sample_count = service->temperature_sample_count;
    summary->pulse_temperature_count = service->pulse_temperature_count;
    summary->followup_temperature_count = service->followup_temperature_count;
    summary->missed_followup_count = service->missed_followup_count;

    summary->pulse_driven_temperature = service->pulse_driven_temperature;
    summary->temperature_valid = service->temperature_valid;
    summary->temperature_first_mdeg_c = service->temperature_first_mdeg_c;
    summary->temperature_current_mdeg_c = service->temperature_current_mdeg_c;
    summary->temperature_min_mdeg_c = service->temperature_min_mdeg_c;
    summary->temperature_max_mdeg_c = service->temperature_max_mdeg_c;
    summary->max_negative_excursion_mdeg_c =
        service->max_negative_excursion_mdeg_c;

    summary->mpu_burst_count = service->mpu_burst_count;
    summary->mpu_sample_count = service->mpu_sample_count;
    summary->mpu_peak_dynamic_accel_mg = service->mpu_peak_dynamic_accel_mg;
    summary->mpu_peak_angular_velocity_dps =
        service->mpu_peak_angular_velocity_dps;
    summary->mpu_total_angular_motion_cdeg =
        SaturateU32(service->mpu_total_angular_motion_cdeg);
    summary->mpu_orientation_valid_count = service->mpu_orientation_valid_count;
    summary->mpu_max_orientation_change_cdeg =
        service->mpu_max_orientation_change_cdeg;

    if (service->mpu_burst_count > 0U)
    {
        summary->mpu_mean_rms_dynamic_accel_mg = SaturateU16(
            service->mpu_rms_dynamic_accel_sum_mg /
            (uint64_t)service->mpu_burst_count);
        summary->mpu_mean_rms_angular_velocity_dps = SaturateU16(
            service->mpu_rms_angular_velocity_sum_dps /
            (uint64_t)service->mpu_burst_count);
    }
}

static void ResetActiveEpisodeData(event_episode_service_t *service)
{
    service->active = false;
    service->active_sequence = 0U;
    service->start_time_ms = 0U;
    service->last_pulse_time_ms = 0U;
    service->close_deadline_ms = 0U;

    service->pulse_count = 0U;
    service->suppressed_retrigger_count = 0U;

    service->followup_active = false;
    service->next_followup_index = 0U;
    service->next_followup_due_ms = 0U;
    service->pulse_driven_temperature = false;

    service->temperature_sample_count = 0U;
    service->pulse_temperature_count = 0U;
    service->followup_temperature_count = 0U;
    service->missed_followup_count = 0U;

    service->temperature_valid = false;
    service->temperature_first_mdeg_c = 0;
    service->temperature_current_mdeg_c = 0;
    service->temperature_min_mdeg_c = 0;
    service->temperature_max_mdeg_c = 0;
    service->max_negative_excursion_mdeg_c = 0;

    service->mpu_burst_count = 0U;
    service->mpu_sample_count = 0U;
    service->mpu_peak_dynamic_accel_mg = 0U;
    service->mpu_rms_dynamic_accel_sum_mg = 0U;
    service->mpu_peak_angular_velocity_dps = 0U;
    service->mpu_rms_angular_velocity_sum_dps = 0U;
    service->mpu_total_angular_motion_cdeg = 0U;
    service->mpu_orientation_valid_count = 0U;
    service->mpu_max_orientation_change_cdeg = 0U;
}

static void CloseEpisode(event_episode_service_t *service)
{
    BuildSummary(service, &service->last_closed_summary);
    service->last_closed_summary_valid = true;
    ResetActiveEpisodeData(service);
}

static void StartEpisode(
    event_episode_service_t *service,
    uint32_t now_ms)
{
    ResetActiveEpisodeData(service);

    if (service->sequence_counter < UINT32_MAX)
    {
        service->sequence_counter++;
    }
    else
    {
        service->sequence_counter = 1U;
    }

    service->active = true;
    service->active_sequence = service->sequence_counter;
    service->start_time_ms = now_ms;
    service->last_pulse_time_ms = now_ms;
    service->close_deadline_ms = now_ms + service->quiet_timeout_ms;

    service->pulse_count = 1U;
    service->followup_active = true;
    service->next_followup_index = 0U;
    service->next_followup_due_ms =
        now_ms + service->temp_followup_offset_ms[0];
}

event_episode_status_t EventEpisodeService_Init(
    event_episode_service_t *service,
    const bolus_runtime_config_t *config)
{
    if ((service == NULL) || (config == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_EPISODE_ERROR_CONFIG;
    }

    memset(service, 0, sizeof(*service));

    service->quiet_timeout_ms =
        (uint32_t)config->event_processing.episode_quiet_timeout_s * 1000UL;
    service->retrigger_guard_ms =
        config->event_processing.episode_retrigger_guard_ms;

    service->temp_followup_offset_ms[0] =
        (uint32_t)config->event_processing.episode_temp_followup_1_s * 1000UL;
    service->temp_followup_offset_ms[1] =
        (uint32_t)config->event_processing.episode_temp_followup_2_s * 1000UL;
    service->temp_followup_offset_ms[2] =
        (uint32_t)config->event_processing.episode_temp_followup_3_s * 1000UL;
    service->temp_followup_offset_ms[3] =
        (uint32_t)config->event_processing.episode_temp_followup_4_s * 1000UL;

    service->initialized = true;
    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_OnMotionPulse(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action)
{
    uint32_t interval_ms = 0U;

    if ((service == NULL) || (action == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    /* Defensive close if caller arrives after a stale quiet deadline. */
    if (service->active && TimeReached(now_ms, service->close_deadline_ms))
    {
        CloseEpisode(service);
        action->episode_closed = true;
    }

    if (!service->active)
    {
        StartEpisode(service, now_ms);

        action->pulse_accepted = true;
        action->episode_started = true;
        action->take_temperature_now = true;
        action->temperature_source = EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE;
        action->take_mpu_burst_now = true;
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        return EVENT_EPISODE_OK;
    }

    interval_ms = now_ms - service->last_pulse_time_ms;

    if ((service->retrigger_guard_ms > 0U) &&
        (interval_ms < service->retrigger_guard_ms))
    {
        IncrementSaturatingU16(&service->suppressed_retrigger_count);
        action->pulse_suppressed_by_guard = true;
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        return EVENT_EPISODE_OK;
    }

    action->pulse_accepted = true;
    action->inter_pulse_interval_valid = true;
    action->inter_pulse_interval_ms = interval_ms;

    service->last_pulse_time_ms = now_ms;
    service->close_deadline_ms = now_ms + service->quiet_timeout_ms;
    IncrementSaturatingU16(&service->pulse_count);

    if (service->pulse_count >= 2U)
    {
        if (service->followup_active)
        {
            service->followup_active = false;
            action->followup_schedule_cancelled = true;
        }

        service->pulse_driven_temperature = true;
    }

    action->take_temperature_now = true;
    action->temperature_source = EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE;
    action->take_mpu_burst_now = true;
    action->episode_sequence = service->active_sequence;
    action->pulse_count = service->pulse_count;

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_Poll(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action)
{
    if ((service == NULL) || (action == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (!service->active)
    {
        return EVENT_EPISODE_OK;
    }

    if (TimeReached(now_ms, service->close_deadline_ms))
    {
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->episode_closed = true;
        CloseEpisode(service);
        return EVENT_EPISODE_OK;
    }

    if (service->followup_active &&
        TimeReached(now_ms, service->next_followup_due_ms))
    {
        uint8_t next_index = service->next_followup_index;

        action->take_temperature_now = true;
        action->temperature_source = EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP;
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;

        next_index++;

        while (next_index < EVENT_EPISODE_TEMP_FOLLOWUP_COUNT)
        {
            uint32_t candidate_due =
                service->start_time_ms +
                service->temp_followup_offset_ms[next_index];

            if (!TimeReached(now_ms, candidate_due))
            {
                break;
            }

            IncrementSaturatingU16(&service->missed_followup_count);
            next_index++;
        }

        if (next_index >= EVENT_EPISODE_TEMP_FOLLOWUP_COUNT)
        {
            service->followup_active = false;
            service->next_followup_index = EVENT_EPISODE_TEMP_FOLLOWUP_COUNT;
            service->next_followup_due_ms = 0U;
        }
        else
        {
            service->next_followup_index = next_index;
            service->next_followup_due_ms =
                service->start_time_ms +
                service->temp_followup_offset_ms[next_index];
        }
    }

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_RecordTemperature(
    event_episode_service_t *service,
    event_episode_temperature_source_t source,
    int32_t temperature_mdeg_c)
{
    int32_t excursion_from_prior_max;

    if (service == NULL)
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if ((!service->active) ||
        (source == EVENT_EPISODE_TEMP_SOURCE_NONE))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!service->temperature_valid)
    {
        service->temperature_valid = true;
        service->temperature_first_mdeg_c = temperature_mdeg_c;
        service->temperature_current_mdeg_c = temperature_mdeg_c;
        service->temperature_min_mdeg_c = temperature_mdeg_c;
        service->temperature_max_mdeg_c = temperature_mdeg_c;
        service->max_negative_excursion_mdeg_c = 0;
    }
    else
    {
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

    IncrementSaturatingU16(&service->temperature_sample_count);

    if ((source == EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE) ||
        (source == EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE))
    {
        IncrementSaturatingU16(&service->pulse_temperature_count);
    }
    else if (source == EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP)
    {
        IncrementSaturatingU16(&service->followup_temperature_count);
    }

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_RecordMpuBurst(
    event_episode_service_t *service,
    const event_episode_mpu_features_t *features)
{
    if ((service == NULL) || (features == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if ((!service->active) || (features->sample_count == 0U))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    IncrementSaturatingU16(&service->mpu_burst_count);

    if ((UINT32_MAX - service->mpu_sample_count) < features->sample_count)
    {
        service->mpu_sample_count = UINT32_MAX;
    }
    else
    {
        service->mpu_sample_count += features->sample_count;
    }

    if (features->peak_dynamic_accel_mg > service->mpu_peak_dynamic_accel_mg)
    {
        service->mpu_peak_dynamic_accel_mg = features->peak_dynamic_accel_mg;
    }

    service->mpu_rms_dynamic_accel_sum_mg +=
        features->rms_dynamic_accel_mg;

    if (features->peak_angular_velocity_dps >
        service->mpu_peak_angular_velocity_dps)
    {
        service->mpu_peak_angular_velocity_dps =
            features->peak_angular_velocity_dps;
    }

    service->mpu_rms_angular_velocity_sum_dps +=
        features->rms_angular_velocity_dps;
    service->mpu_total_angular_motion_cdeg +=
        features->total_angular_motion_cdeg;

    if (features->orientation_change_valid)
    {
        IncrementSaturatingU16(&service->mpu_orientation_valid_count);

        if (features->orientation_change_cdeg >
            service->mpu_max_orientation_change_cdeg)
        {
            service->mpu_max_orientation_change_cdeg =
                features->orientation_change_cdeg;
        }
    }

    return EVENT_EPISODE_OK;
}

bool EventEpisodeService_IsActive(const event_episode_service_t *service)
{
    return ((service != NULL) && service->initialized && service->active);
}

event_episode_status_t EventEpisodeService_GetActiveSummary(
    const event_episode_service_t *service,
    event_episode_summary_t *summary)
{
    if ((service == NULL) || (summary == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (!service->active)
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    BuildSummary(service, summary);
    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_GetLastClosedSummary(
    const event_episode_service_t *service,
    event_episode_summary_t *summary)
{
    if ((service == NULL) || (summary == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (!service->last_closed_summary_valid)
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    *summary = service->last_closed_summary;
    return EVENT_EPISODE_OK;
}
