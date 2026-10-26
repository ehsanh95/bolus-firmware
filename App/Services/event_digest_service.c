#include "event_digest_service.h"

#include <limits.h>
#include <string.h>

static uint8_t U8(uint32_t value)
{
    return (value > UINT8_MAX) ? UINT8_MAX : (uint8_t)value;
}

static uint16_t U16(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
}

static int16_t CentiC(int32_t mdeg_c)
{
    int32_t value =
        (mdeg_c >= 0) ? ((mdeg_c + 5) / 10) : ((mdeg_c - 5) / 10);

    if (value > INT16_MAX)
        value = INT16_MAX;
    if (value < INT16_MIN)
        value = INT16_MIN;

    return (int16_t)value;
}

void EventDigestService_Init(event_digest_service_t *service)
{
    if (service != NULL)
    {
        memset(service, 0, sizeof(*service));
    }
}

bool EventDigestService_PushClosedEpisode(
    event_digest_service_t *service,
    const event_episode_summary_t *episode,
    uint32_t window_start_ms)
{
    bolus_event_digest_t *digest;
    uint32_t offset_ms;

    if ((service == NULL) || (episode == NULL))
    {
        return false;
    }

    if (service->count >= EVENT_DIGEST_CAPACITY)
    {
        service->overflow_count++;
        service->overflow_pending_count++;
        service->overflow_latched = true;
        return false;
    }

    digest = &service->items[service->count];
    memset(digest, 0, sizeof(*digest));

    if ((int32_t)(episode->start_time_ms - window_start_ms) < 0)
    {
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_CARRY;
    }
    else
    {
        offset_ms = episode->start_time_ms - window_start_ms;
        digest->start_offset_s = U16((offset_ms + 500U) / 1000U);
    }

    if (episode->carry_from_previous_window)
    {
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_CARRY;
    }

    digest->duration_s =
        U16((episode->active_span_ms + 500U) / 1000U);
    digest->pulse_count = U8(episode->pulse_count);
    digest->mean_inter_pulse_interval_s =
        U8((episode->inter_pulse_interval_mean_ms + 500U) / 1000U);
    digest->suppressed_trigger_count =
        U8(episode->suppressed_retrigger_count);
    digest->close_reason = (uint8_t)episode->close_reason;
    digest->mpu_burst_count = U8(episode->mpu_burst_count);

    if (episode->temperature_valid)
    {
        digest->temperature_delta_centi_c =
            CentiC(
                episode->temperature_current_mdeg_c -
                episode->temperature_first_mdeg_c);
        digest->temperature_min_centi_c =
            CentiC(episode->temperature_min_mdeg_c);
        digest->temperature_max_centi_c =
            CentiC(episode->temperature_max_mdeg_c);
        digest->temperature_end_centi_c =
            CentiC(episode->temperature_current_mdeg_c);
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_TEMP_VALID;
    }

    digest->rms_dynamic_accel_mg =
        episode->mpu_mean_rms_dynamic_accel_mg;
    digest->peak_dynamic_accel_mg =
        episode->mpu_peak_dynamic_accel_mg;
    digest->peak_angular_velocity_dps =
        episode->mpu_peak_angular_velocity_dps;
    digest->orientation_change_cdeg =
        episode->mpu_max_orientation_change_cdeg;

    if ((episode->source_flags & EVENT_EPISODE_SOURCE_MOTION) != 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_MOTION;
    if ((episode->source_flags & EVENT_EPISODE_SOURCE_TEMP_HIGH) != 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_TEMP_HIGH;
    if ((episode->source_flags & EVENT_EPISODE_SOURCE_TEMP_LOW) != 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_TEMP_LOW;
    if (episode->mpu_burst_count > 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_MPU_USED;
    if (episode->mpu_skipped_by_policy_count > 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_MPU_SKIPPED;
    if (episode->mpu_failure_count > 0U)
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_MPU_FAILED;

    service->count++;
    service->pushed_count++;
    return true;
}

bool EventDigestService_PushBaseline(
    event_digest_service_t *service,
    uint32_t window_start_ms,
    uint32_t window_end_ms,
    bool temperature_valid,
    int32_t temperature_min_mdeg_c,
    int32_t temperature_max_mdeg_c,
    int32_t temperature_end_mdeg_c)
{
    bolus_event_digest_t *digest;
    uint32_t duration_ms;

    if (service == NULL)
    {
        return false;
    }

    if (service->count >= EVENT_DIGEST_CAPACITY)
    {
        service->overflow_count++;
        service->overflow_latched = true;
        return false;
    }

    digest = &service->items[service->count];
    memset(digest, 0, sizeof(*digest));

    duration_ms = window_end_ms - window_start_ms;
    digest->start_offset_s = 0U;
    digest->duration_s = U16((duration_ms + 500U) / 1000U);
    digest->close_reason = BOLUS_EVENT_CLOSE_TELEMETRY_BOUNDARY;

    if (temperature_valid)
    {
        digest->temperature_min_centi_c =
            CentiC(temperature_min_mdeg_c);
        digest->temperature_max_centi_c =
            CentiC(temperature_max_mdeg_c);
        digest->temperature_end_centi_c =
            CentiC(temperature_end_mdeg_c);
        digest->temperature_delta_centi_c = 0;
        digest->flags |= BOLUS_EVENT_DIGEST_FLAG_TEMP_VALID;
    }

    service->count++;
    service->pushed_count++;
    return true;
}

uint8_t EventDigestService_SnapshotAndClear(
    event_digest_service_t *service,
    bolus_event_digest_t *destination,
    uint8_t capacity)
{
    uint8_t count;

    if ((service == NULL) || (destination == NULL) || (capacity == 0U))
    {
        return 0U;
    }

    count = (service->count < capacity) ? service->count : capacity;
    memcpy(destination, service->items, (size_t)count * sizeof(*destination));

    if (count < service->count)
    {
        service->overflow_count += (uint32_t)(service->count - count);
        service->overflow_pending_count += (uint32_t)(service->count - count);
        service->overflow_latched = true;
    }

    memset(service->items, 0, sizeof(service->items));
    service->count = 0U;
    return count;
}

bool EventDigestService_ConsumeOverflow(event_digest_service_t *service)
{
    bool value;

    if (service == NULL)
    {
        return false;
    }

    value = service->overflow_latched;
    service->overflow_latched = false;
    return value;
}

uint8_t EventDigestService_ConsumeDroppedCount(event_digest_service_t *service)
{
    uint32_t value;

    if (service == NULL)
    {
        return 0U;
    }

    value = service->overflow_pending_count;
    service->overflow_pending_count = 0U;

    return (value > UINT8_MAX) ? UINT8_MAX : (uint8_t)value;
}
