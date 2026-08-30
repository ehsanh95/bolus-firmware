#include "event_feature_extractor.h"

#include <limits.h>
#include <string.h>

#define EVENT_FEATURE_GRAVITY_MG  1000UL

static uint32_t IntegerSqrtU64(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = (uint64_t)1U << 62;

    while (bit > value)
    {
        bit >>= 2;
    }

    while (bit != 0U)
    {
        if (value >= (result + bit))
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }

        bit >>= 2;
    }

    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t)result;
}

static uint16_t SaturateU16(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
}

static uint32_t VectorMagnitude3(
    int32_t x,
    int32_t y,
    int32_t z)
{
    uint64_t sum_sq =
        ((uint64_t)((int64_t)x * (int64_t)x)) +
        ((uint64_t)((int64_t)y * (int64_t)y)) +
        ((uint64_t)((int64_t)z * (int64_t)z));

    return IntegerSqrtU64(sum_sq);
}

event_feature_extractor_status_t EventFeatureExtractor_Start(
    event_feature_extractor_t *extractor,
    uint32_t event_sequence,
    uint32_t event_timestamp_s,
    uint32_t start_tick_ms)
{
    if (extractor == NULL)
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_PARAM;
    }

    memset(extractor, 0, sizeof(*extractor));

    extractor->active = true;
    extractor->event_sequence = event_sequence;
    extractor->event_timestamp_s = event_timestamp_s;
    extractor->start_tick_ms = start_tick_ms;
    extractor->last_tick_ms = start_tick_ms;

    return EVENT_FEATURE_EXTRACTOR_OK;
}

event_feature_extractor_status_t EventFeatureExtractor_AddBmaSample(
    event_feature_extractor_t *extractor,
    int16_t x_mg,
    int16_t y_mg,
    int16_t z_mg,
    uint32_t sample_tick_ms)
{
    uint32_t accel_magnitude_mg;
    uint32_t dynamic_magnitude_mg;

    if (extractor == NULL)
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_PARAM;
    }

    if (!extractor->active)
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_STATE;
    }

    accel_magnitude_mg = VectorMagnitude3(x_mg, y_mg, z_mg);
    dynamic_magnitude_mg =
        (accel_magnitude_mg >= EVENT_FEATURE_GRAVITY_MG)
            ? (accel_magnitude_mg - EVENT_FEATURE_GRAVITY_MG)
            : (EVENT_FEATURE_GRAVITY_MG - accel_magnitude_mg);

    if (dynamic_magnitude_mg > extractor->peak_dynamic_accel_mg)
    {
        extractor->peak_dynamic_accel_mg = SaturateU16(dynamic_magnitude_mg);
    }

    extractor->dynamic_sum_mg += dynamic_magnitude_mg;
    extractor->dynamic_sum_sq_mg2 +=
        (uint64_t)dynamic_magnitude_mg * (uint64_t)dynamic_magnitude_mg;

    if (extractor->sample_count < UINT16_MAX)
    {
        extractor->sample_count++;
    }

    if (extractor->previous_sample_valid)
    {
        uint32_t dt_ms = sample_tick_ms - extractor->last_tick_ms;

        if (dt_ms > 0U)
        {
            int32_t dx = (int32_t)x_mg - (int32_t)extractor->previous_x_mg;
            int32_t dy = (int32_t)y_mg - (int32_t)extractor->previous_y_mg;
            int32_t dz = (int32_t)z_mg - (int32_t)extractor->previous_z_mg;
            uint32_t delta_accel_mg = VectorMagnitude3(dx, dy, dz);
            uint64_t jerk_scaled =
                ((uint64_t)delta_accel_mg * 1000ULL) / (uint64_t)dt_ms;
            uint32_t jerk_mg_s =
                (jerk_scaled > UINT32_MAX) ? UINT32_MAX : (uint32_t)jerk_scaled;

            if (jerk_mg_s > extractor->peak_jerk_mg_s)
            {
                extractor->peak_jerk_mg_s = jerk_mg_s;
            }

            extractor->jerk_sum_sq_mg2_s2 +=
                (uint64_t)jerk_mg_s * (uint64_t)jerk_mg_s;

            if (extractor->jerk_sample_count < UINT16_MAX)
            {
                extractor->jerk_sample_count++;
            }
        }
    }

    extractor->previous_sample_valid = true;
    extractor->previous_x_mg = x_mg;
    extractor->previous_y_mg = y_mg;
    extractor->previous_z_mg = z_mg;
    extractor->last_tick_ms = sample_tick_ms;

    return EVENT_FEATURE_EXTRACTOR_OK;
}

bool EventFeatureExtractor_IsMotionWindowComplete(
    const event_feature_extractor_t *extractor,
    uint32_t now_tick_ms)
{
    if ((extractor == NULL) || (!extractor->active))
    {
        return false;
    }

    return ((now_tick_ms - extractor->start_tick_ms) >=
            BOLUS_EVENT_MOTION_CAPTURE_TARGET_MS);
}

event_feature_extractor_status_t EventFeatureExtractor_FinalizeMotion(
    event_feature_extractor_t *extractor,
    uint32_t now_tick_ms,
    bolus_event_feature_record_t *record)
{
    uint32_t elapsed_ms;
    uint64_t mean_dynamic_mg;
    uint64_t mean_dynamic_sq_mg2;
    uint64_t variance_mg2;

    if ((extractor == NULL) || (record == NULL))
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_PARAM;
    }

    if ((!extractor->active) || (extractor->sample_count == 0U))
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_STATE;
    }

    if (!EventFeatureExtractor_IsMotionWindowComplete(extractor, now_tick_ms))
    {
        return EVENT_FEATURE_EXTRACTOR_NOT_COMPLETE;
    }

    memset(record, 0, sizeof(*record));

    elapsed_ms = now_tick_ms - extractor->start_tick_ms;

    record->event_sequence = extractor->event_sequence;
    record->timestamp_s = extractor->event_timestamp_s;
    record->capture_duration_ms = SaturateU16(elapsed_ms);
    record->sample_count = extractor->sample_count;
    record->peak_dynamic_accel_mg = extractor->peak_dynamic_accel_mg;

    mean_dynamic_mg =
        extractor->dynamic_sum_mg / (uint64_t)extractor->sample_count;
    mean_dynamic_sq_mg2 =
        extractor->dynamic_sum_sq_mg2 / (uint64_t)extractor->sample_count;

    variance_mg2 =
        (mean_dynamic_sq_mg2 >= (mean_dynamic_mg * mean_dynamic_mg))
            ? (mean_dynamic_sq_mg2 - (mean_dynamic_mg * mean_dynamic_mg))
            : 0U;

    record->rms_dynamic_accel_mg =
        SaturateU16(IntegerSqrtU64(mean_dynamic_sq_mg2));
    record->accel_variance_mg2 =
        (variance_mg2 > UINT32_MAX) ? UINT32_MAX : (uint32_t)variance_mg2;

    record->peak_jerk_mg_s = extractor->peak_jerk_mg_s;
    if (extractor->jerk_sample_count > 0U)
    {
        record->rms_jerk_mg_s = IntegerSqrtU64(
            extractor->jerk_sum_sq_mg2_s2 /
            (uint64_t)extractor->jerk_sample_count);
    }

    extractor->active = false;

    return EVENT_FEATURE_EXTRACTOR_OK;
}

event_feature_extractor_status_t EventFeatureRecord_SetTemperature(
    bolus_event_feature_record_t *record,
    bolus_event_temperature_point_t point,
    int32_t temperature_mdeg_c)
{
    if ((record == NULL) || (point >= BOLUS_EVENT_TEMP_POINT_COUNT))
    {
        return EVENT_FEATURE_EXTRACTOR_ERROR_PARAM;
    }

    record->temperature_mdeg_c[point] = temperature_mdeg_c;
    record->temperature_valid_mask |= (uint8_t)(1U << point);

    return EVENT_FEATURE_EXTRACTOR_OK;
}
