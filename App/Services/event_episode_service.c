#include "event_episode_service.h"

#include "stm32l4xx_hal.h"

#include <limits.h>
#include <string.h>

static bool Reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void ClearAction(event_episode_action_t *action)
{
    memset(action, 0, sizeof(*action));
    action->temperature_source = EVENT_EPISODE_TEMP_SOURCE_NONE;
}

static void IncrementU16(uint16_t *value)
{
    if (*value < UINT16_MAX)
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

static bool ShouldTakeMpu(bolus_acquisition_level_t level, uint16_t pulse_count)
{
    switch (level)
    {
        case BOLUS_ACQUISITION_LEVEL_0:
            return false;
        case BOLUS_ACQUISITION_LEVEL_1:
            return pulse_count == 1U;
        case BOLUS_ACQUISITION_LEVEL_2:
            return (pulse_count == 1U) || ((pulse_count % 4U) == 0U);
        case BOLUS_ACQUISITION_LEVEL_3:
            return (pulse_count == 1U) || ((pulse_count % 3U) == 0U);
        case BOLUS_ACQUISITION_LEVEL_4:
            return (pulse_count == 1U) || ((pulse_count % 2U) == 0U);
        case BOLUS_ACQUISITION_LEVEL_5:
            return true;
        default:
            return false;
    }
}

static void BuildSummary(
    const event_episode_service_t *service,
    uint32_t end_time_ms,
    event_episode_close_reason_t close_reason,
    event_episode_summary_t *summary)
{
    uint64_t mean = 0U;
    uint64_t mean_square = 0U;
    uint64_t variance = 0U;

    memset(summary, 0, sizeof(*summary));

    summary->episode_sequence = service->active_sequence;
    summary->start_time_ms = service->start_time_ms;
    summary->last_pulse_time_ms = service->last_pulse_time_ms;
    summary->close_deadline_ms = service->close_deadline_ms;
    summary->active_span_ms = end_time_ms - service->start_time_ms;
    summary->source_flags = service->source_flags;
    summary->active_thermal_flags = service->active_thermal_flags;
    summary->thermal_active = service->thermal_active;
    summary->carry_from_previous_window =
        service->carry_from_previous_window;
    summary->close_reason = close_reason;

    summary->pulse_count = service->pulse_count;
    summary->suppressed_retrigger_count =
        service->suppressed_retrigger_count;
    summary->inter_pulse_interval_count =
        service->inter_pulse_interval_count;

    if (service->inter_pulse_interval_count > 0U)
    {
        mean =
            service->inter_pulse_interval_sum_ms /
            service->inter_pulse_interval_count;
        mean_square =
            service->inter_pulse_interval_sum_sq_ms2 /
            service->inter_pulse_interval_count;
        variance =
            (mean_square >= (mean * mean)) ?
            (mean_square - (mean * mean)) : 0U;

        summary->inter_pulse_interval_mean_ms = SaturateU32(mean);
        summary->inter_pulse_interval_variance_ms2 =
            SaturateU32(variance);
    }

    summary->temperature_sample_count = service->temperature_sample_count;
    summary->pulse_temperature_count = service->pulse_temperature_count;
    summary->followup_temperature_count =
        service->followup_temperature_count;
    summary->missed_followup_count = service->missed_followup_count;
    summary->pulse_driven_temperature = service->pulse_driven_temperature;
    summary->temperature_valid = service->temperature_valid;
    summary->temperature_first_mdeg_c =
        service->temperature_first_mdeg_c;
    summary->temperature_current_mdeg_c =
        service->temperature_current_mdeg_c;
    summary->temperature_min_mdeg_c = service->temperature_min_mdeg_c;
    summary->temperature_max_mdeg_c = service->temperature_max_mdeg_c;
    summary->max_negative_excursion_mdeg_c =
        service->max_negative_excursion_mdeg_c;

    summary->mpu_burst_count = service->mpu_burst_count;
    summary->mpu_skipped_by_policy_count =
        service->mpu_skipped_by_policy_count;
    summary->mpu_failure_count = service->mpu_failure_count;
    summary->mpu_sample_count = service->mpu_sample_count;
    summary->mpu_peak_dynamic_accel_mg =
        service->mpu_peak_dynamic_accel_mg;
    summary->mpu_peak_angular_velocity_dps =
        service->mpu_peak_angular_velocity_dps;
    summary->mpu_total_angular_motion_cdeg =
        SaturateU32(service->mpu_total_angular_motion_cdeg);
    summary->mpu_orientation_valid_count =
        service->mpu_orientation_valid_count;
    summary->mpu_max_orientation_change_cdeg =
        service->mpu_max_orientation_change_cdeg;

    if (service->mpu_burst_count > 0U)
    {
        summary->mpu_mean_rms_dynamic_accel_mg =
            SaturateU16(
                service->mpu_rms_dynamic_accel_sum_mg /
                service->mpu_burst_count);
        summary->mpu_mean_rms_angular_velocity_dps =
            SaturateU16(
                service->mpu_rms_angular_velocity_sum_dps /
                service->mpu_burst_count);
    }
}

static void ResetActive(event_episode_service_t *service)
{
    uint32_t sequence_counter = service->sequence_counter;
    bolus_acquisition_level_t acquisition_level =
        service->acquisition_level;
    uint32_t quiet_timeout_ms = service->quiet_timeout_ms;
    uint32_t retrigger_guard_ms = service->retrigger_guard_ms;
    uint32_t max_duration_ms = service->max_duration_ms;
    uint32_t followups[EVENT_EPISODE_TEMP_FOLLOWUP_COUNT];
    bool initialized = service->initialized;
    bool last_valid = service->last_closed_summary_valid;
    event_episode_summary_t last = service->last_closed_summary;

    memcpy(followups, service->temp_followup_offset_ms, sizeof(followups));
    memset(service, 0, sizeof(*service));

    service->sequence_counter = sequence_counter;
    service->acquisition_level = acquisition_level;
    service->quiet_timeout_ms = quiet_timeout_ms;
    service->retrigger_guard_ms = retrigger_guard_ms;
    service->max_duration_ms = max_duration_ms;
    memcpy(service->temp_followup_offset_ms, followups, sizeof(followups));
    service->initialized = initialized;
    service->last_closed_summary_valid = last_valid;
    service->last_closed_summary = last;
}

static void CloseAt(
    event_episode_service_t *service,
    uint32_t end_time_ms,
    event_episode_close_reason_t reason)
{
    BuildSummary(
        service,
        end_time_ms,
        reason,
        &service->last_closed_summary);
    service->last_closed_summary_valid = true;
    ResetActive(service);
}

static void Start(
    event_episode_service_t *service,
    uint32_t now_ms,
    uint8_t source_flags,
    bool motion_pulse,
    bool carry)
{
    service->sequence_counter =
        (service->sequence_counter < UINT32_MAX) ?
        (service->sequence_counter + 1U) : 1U;
    service->active = true;
    service->active_sequence = service->sequence_counter;
    service->start_time_ms = now_ms;
    service->last_activity_time_ms = now_ms;
    service->last_pulse_time_ms = motion_pulse ? now_ms : 0U;
    service->close_deadline_ms = now_ms + service->quiet_timeout_ms;
    service->max_deadline_ms = now_ms + service->max_duration_ms;
    service->source_flags = source_flags;
    service->active_thermal_flags =
        source_flags &
        (EVENT_EPISODE_SOURCE_TEMP_HIGH | EVENT_EPISODE_SOURCE_TEMP_LOW);
    service->thermal_active = service->active_thermal_flags != 0U;
    service->carry_from_previous_window = carry;

    if (motion_pulse)
    {
        service->pulse_count = 1U;
        service->first_motion_time_ms = now_ms;
        service->followup_active = true;
        service->next_followup_due_ms =
            now_ms + service->temp_followup_offset_ms[0];
    }
}

static void CaptureCarry(
    const event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_carry_state_t *carry)
{
    memset(carry, 0, sizeof(*carry));

    if (!service->active)
    {
        return;
    }

    carry->motion_active =
        ((service->source_flags & EVENT_EPISODE_SOURCE_MOTION) != 0U) &&
        !Reached(now_ms, service->close_deadline_ms);
    carry->thermal_active =
        service->thermal_active &&
        (service->active_thermal_flags != 0U);
    carry->thermal_flags =
        carry->thermal_active ? service->active_thermal_flags : 0U;
    carry->last_pulse_time_ms = service->last_pulse_time_ms;
    carry->motion_close_deadline_ms = service->close_deadline_ms;
    carry->valid = carry->motion_active || carry->thermal_active;
}

static bool StartCarryInternal(
    event_episode_service_t *service,
    uint32_t now_ms,
    const event_episode_carry_state_t *carry)
{
    uint8_t flags = 0U;

    if ((carry == NULL) || !carry->valid)
    {
        return false;
    }

    if (carry->thermal_active)
    {
        flags |= carry->thermal_flags &
            (EVENT_EPISODE_SOURCE_TEMP_HIGH |
             EVENT_EPISODE_SOURCE_TEMP_LOW);
    }

    if (carry->motion_active &&
        !Reached(now_ms, carry->motion_close_deadline_ms))
    {
        flags |= EVENT_EPISODE_SOURCE_MOTION;
    }

    if (flags == 0U)
    {
        return false;
    }

    Start(service, now_ms, flags, false, true);

    if ((flags & EVENT_EPISODE_SOURCE_MOTION) != 0U)
    {
        service->last_pulse_time_ms = carry->last_pulse_time_ms;
        service->close_deadline_ms = carry->motion_close_deadline_ms;
    }

    service->active_thermal_flags =
        flags &
        (EVENT_EPISODE_SOURCE_TEMP_HIGH | EVENT_EPISODE_SOURCE_TEMP_LOW);
    service->thermal_active = service->active_thermal_flags != 0U;
    return true;
}

static void MarkActionState(
    const event_episode_service_t *service,
    event_episode_action_t *action)
{
    if (service->active)
    {
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
    }
}

event_episode_status_t EventEpisodeService_Init(
    event_episode_service_t *service,
    const bolus_runtime_config_t *config)
{
    uint32_t sequence_counter;

    if ((service == NULL) || (config == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_EPISODE_ERROR_CONFIG;
    }

    sequence_counter = service->sequence_counter;
    memset(service, 0, sizeof(*service));
    service->sequence_counter = sequence_counter;
    service->acquisition_level = config->acquisition_level;
    service->quiet_timeout_ms =
        (uint32_t)config->event_processing.episode_quiet_timeout_s * 1000UL;
    service->retrigger_guard_ms =
        config->event_processing.episode_retrigger_guard_ms;
    service->max_duration_ms =
        (uint32_t)config->event_processing.episode_max_duration_s * 1000UL;
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
    bool take_mpu;
    event_episode_carry_state_t carry = {0};

    if ((service == NULL) || (action == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (service->active && Reached(now_ms, service->max_deadline_ms))
    {
        CaptureCarry(service, service->max_deadline_ms, &carry);
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
        CloseAt(
            service,
            service->max_deadline_ms,
            EVENT_EPISODE_CLOSE_MAX_DURATION);
        action->episode_closed = true;

        if (StartCarryInternal(
                service,
                service->last_closed_summary.start_time_ms +
                    service->last_closed_summary.active_span_ms,
                &carry))
        {
            action->episode_started = true;
        }
    }

    if (service->active &&
        ((service->source_flags & EVENT_EPISODE_SOURCE_MOTION) != 0U) &&
        Reached(now_ms, service->close_deadline_ms))
    {
        uint32_t split_time = service->close_deadline_ms;

        memset(&carry, 0, sizeof(carry));
        carry.thermal_active = service->thermal_active;
        carry.thermal_flags = service->active_thermal_flags;
        carry.valid = carry.thermal_active;

        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
        CloseAt(
            service,
            split_time,
            EVENT_EPISODE_CLOSE_MOTION_QUIET);
        action->episode_closed = true;

        if (StartCarryInternal(service, split_time, &carry))
        {
            action->episode_started = true;
        }
    }

    if (!service->active)
    {
        Start(service, now_ms, EVENT_EPISODE_SOURCE_MOTION, true, false);
        action->episode_started = true;
        action->pulse_accepted = true;
        action->take_temperature_now = true;
        action->temperature_source =
            EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE;
    }
    else
    {
        if (service->pulse_count > 0U)
        {
            interval_ms = now_ms - service->last_pulse_time_ms;
        }

        if ((service->pulse_count > 0U) &&
            (service->retrigger_guard_ms > 0U) &&
            (interval_ms < service->retrigger_guard_ms))
        {
            IncrementU16(&service->suppressed_retrigger_count);
            action->pulse_suppressed_by_guard = true;
            MarkActionState(service, action);
            return EVENT_EPISODE_OK;
        }

        action->pulse_accepted = true;

        if (service->pulse_count > 0U)
        {
            action->inter_pulse_interval_valid = true;
            action->inter_pulse_interval_ms = interval_ms;
            IncrementU16(&service->inter_pulse_interval_count);
            service->inter_pulse_interval_sum_ms += interval_ms;
            service->inter_pulse_interval_sum_sq_ms2 +=
                (uint64_t)interval_ms * interval_ms;
        }

        service->last_pulse_time_ms = now_ms;
        service->last_activity_time_ms = now_ms;
        service->close_deadline_ms = now_ms + service->quiet_timeout_ms;
        service->source_flags |= EVENT_EPISODE_SOURCE_MOTION;

        if (service->pulse_count == 0U)
        {
            service->first_motion_time_ms = now_ms;
            service->followup_active = true;
            service->next_followup_index = 0U;
            service->next_followup_due_ms =
                now_ms + service->temp_followup_offset_ms[0];
            action->take_temperature_now = true;
            action->temperature_source =
                EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE;
        }

        IncrementU16(&service->pulse_count);

        if ((service->pulse_count > 1U) &&
            (service->acquisition_level >= BOLUS_ACQUISITION_LEVEL_4))
        {
            action->take_temperature_now = true;
            action->temperature_source =
                EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE;
            service->pulse_driven_temperature = true;
        }
    }

    take_mpu =
        ShouldTakeMpu(service->acquisition_level, service->pulse_count);
    action->take_mpu_burst_now = take_mpu;

    if (!take_mpu)
    {
        action->mpu_skipped_by_policy = true;
        IncrementU16(&service->mpu_skipped_by_policy_count);
    }

    MarkActionState(service, action);
    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_OnThermalAlert(
    event_episode_service_t *service,
    uint32_t now_ms,
    bool high,
    bool low,
    event_episode_action_t *action)
{
    uint8_t thermal_flags = 0U;
    event_episode_carry_state_t carry = {0};

    if ((service == NULL) || (action == NULL) ||
        ((!high) && (!low)) || (high && low))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (high)
    {
        thermal_flags |= EVENT_EPISODE_SOURCE_TEMP_HIGH;
    }
    if (low)
    {
        thermal_flags |= EVENT_EPISODE_SOURCE_TEMP_LOW;
    }

    if (service->active && Reached(now_ms, service->max_deadline_ms))
    {
        CaptureCarry(service, service->max_deadline_ms, &carry);
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
        CloseAt(
            service,
            service->max_deadline_ms,
            EVENT_EPISODE_CLOSE_MAX_DURATION);
        action->episode_closed = true;

        if (StartCarryInternal(
                service,
                service->last_closed_summary.start_time_ms +
                    service->last_closed_summary.active_span_ms,
                &carry))
        {
            action->episode_started = true;
        }
    }

    if (!service->active)
    {
        Start(service, now_ms, thermal_flags, false, false);
        action->episode_started = true;
    }
    else
    {
        service->source_flags |= thermal_flags;
        service->active_thermal_flags = thermal_flags;
        service->thermal_active = true;
        service->last_activity_time_ms = now_ms;
    }

    MarkActionState(service, action);
    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_OnThermalNormal(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action)
{
    event_episode_carry_state_t carry = {0};

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

    service->thermal_active = false;
    service->active_thermal_flags = 0U;
    service->last_activity_time_ms = now_ms;

    if (((service->source_flags & EVENT_EPISODE_SOURCE_MOTION) != 0U) &&
        !Reached(now_ms, service->close_deadline_ms))
    {
        carry.valid = true;
        carry.motion_active = true;
        carry.last_pulse_time_ms = service->last_pulse_time_ms;
        carry.motion_close_deadline_ms = service->close_deadline_ms;
    }

    action->episode_sequence = service->active_sequence;
    action->pulse_count = service->pulse_count;
    action->source_flags = service->source_flags;
    CloseAt(service, now_ms, EVENT_EPISODE_CLOSE_TEMP_NORMAL);
    action->episode_closed = true;

    if (StartCarryInternal(service, now_ms, &carry))
    {
        action->episode_started = true;
    }

    MarkActionState(service, action);
    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_Poll(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_action_t *action)
{
    uint8_t next_index;
    event_episode_carry_state_t carry = {0};

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

    if (Reached(now_ms, service->max_deadline_ms))
    {
        uint32_t split_time = service->max_deadline_ms;

        CaptureCarry(service, split_time, &carry);
        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
        CloseAt(
            service,
            split_time,
            EVENT_EPISODE_CLOSE_MAX_DURATION);
        action->episode_closed = true;

        if (StartCarryInternal(service, split_time, &carry))
        {
            action->episode_started = true;
            MarkActionState(service, action);
        }

        return EVENT_EPISODE_OK;
    }

    if (((service->source_flags & EVENT_EPISODE_SOURCE_MOTION) != 0U) &&
        Reached(now_ms, service->close_deadline_ms))
    {
        uint32_t split_time = service->close_deadline_ms;

        carry.thermal_active = service->thermal_active;
        carry.thermal_flags = service->active_thermal_flags;
        carry.valid = carry.thermal_active;

        action->episode_sequence = service->active_sequence;
        action->pulse_count = service->pulse_count;
        action->source_flags = service->source_flags;
        CloseAt(
            service,
            split_time,
            EVENT_EPISODE_CLOSE_MOTION_QUIET);
        action->episode_closed = true;

        if (StartCarryInternal(service, split_time, &carry))
        {
            action->episode_started = true;
            MarkActionState(service, action);
        }

        return EVENT_EPISODE_OK;
    }

    if (service->followup_active &&
        Reached(now_ms, service->next_followup_due_ms))
    {
        action->take_temperature_now = true;
        action->temperature_source =
            EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP;
        MarkActionState(service, action);

        next_index = (uint8_t)(service->next_followup_index + 1U);
        while ((next_index < EVENT_EPISODE_TEMP_FOLLOWUP_COUNT) &&
               Reached(
                   now_ms,
                   service->first_motion_time_ms +
                       service->temp_followup_offset_ms[next_index]))
        {
            IncrementU16(&service->missed_followup_count);
            next_index++;
        }

        if (next_index >= EVENT_EPISODE_TEMP_FOLLOWUP_COUNT)
        {
            service->followup_active = false;
            service->next_followup_index = next_index;
            service->next_followup_due_ms = 0U;
        }
        else
        {
            service->next_followup_index = next_index;
            service->next_followup_due_ms =
                service->first_motion_time_ms +
                service->temp_followup_offset_ms[next_index];
        }
    }

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_ForceClose(
    event_episode_service_t *service,
    uint32_t now_ms,
    event_episode_close_reason_t reason,
    event_episode_carry_state_t *carry,
    event_episode_action_t *action)
{
    if ((service == NULL) || (carry == NULL) || (action == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);
    memset(carry, 0, sizeof(*carry));

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (!service->active)
    {
        return EVENT_EPISODE_OK;
    }

    CaptureCarry(service, now_ms, carry);
    action->episode_sequence = service->active_sequence;
    action->pulse_count = service->pulse_count;
    action->source_flags = service->source_flags;
    CloseAt(service, now_ms, reason);
    action->episode_closed = true;

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_StartCarry(
    event_episode_service_t *service,
    uint32_t now_ms,
    const event_episode_carry_state_t *carry,
    event_episode_action_t *action)
{
    if ((service == NULL) || (carry == NULL) || (action == NULL))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    ClearAction(action);

    if (!service->initialized)
    {
        return EVENT_EPISODE_ERROR_NOT_INITIALIZED;
    }

    if (service->active)
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    if (StartCarryInternal(service, now_ms, carry))
    {
        action->episode_started = true;
        MarkActionState(service, action);
    }

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_RecordTemperature(
    event_episode_service_t *service,
    event_episode_temperature_source_t source,
    int32_t temperature_mdeg_c)
{
    int32_t excursion;

    if ((service == NULL) || !service->initialized || !service->active ||
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
    }
    else
    {
        excursion =
            temperature_mdeg_c - service->temperature_max_mdeg_c;
        if (excursion < service->max_negative_excursion_mdeg_c)
        {
            service->max_negative_excursion_mdeg_c = excursion;
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

    service->last_activity_time_ms = HAL_GetTick();
    IncrementU16(&service->temperature_sample_count);

    if ((source == EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE) ||
        (source == EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE))
    {
        IncrementU16(&service->pulse_temperature_count);
    }
    else if (source == EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP)
    {
        IncrementU16(&service->followup_temperature_count);
    }

    return EVENT_EPISODE_OK;
}

event_episode_status_t EventEpisodeService_RecordMpuBurst(
    event_episode_service_t *service,
    const event_episode_mpu_features_t *features)
{
    if ((service == NULL) || (features == NULL) ||
        !service->initialized || !service->active ||
        (features->sample_count == 0U))
    {
        return EVENT_EPISODE_ERROR_PARAM;
    }

    IncrementU16(&service->mpu_burst_count);
    service->mpu_sample_count =
        (UINT32_MAX - service->mpu_sample_count < features->sample_count) ?
        UINT32_MAX :
        (service->mpu_sample_count + features->sample_count);

    if (features->peak_dynamic_accel_mg >
        service->mpu_peak_dynamic_accel_mg)
    {
        service->mpu_peak_dynamic_accel_mg =
            features->peak_dynamic_accel_mg;
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
        IncrementU16(&service->mpu_orientation_valid_count);
        if (features->orientation_change_cdeg >
            service->mpu_max_orientation_change_cdeg)
        {
            service->mpu_max_orientation_change_cdeg =
                features->orientation_change_cdeg;
        }
    }

    service->last_activity_time_ms = HAL_GetTick();
    return EVENT_EPISODE_OK;
}

void EventEpisodeService_MarkMpuFailure(event_episode_service_t *service)
{
    if ((service != NULL) && service->initialized && service->active)
    {
        IncrementU16(&service->mpu_failure_count);
    }
}

bool EventEpisodeService_IsActive(const event_episode_service_t *service)
{
    return (service != NULL) && service->initialized && service->active;
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

    BuildSummary(
        service,
        service->last_activity_time_ms,
        EVENT_EPISODE_CLOSE_UNSPECIFIED,
        summary);
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
