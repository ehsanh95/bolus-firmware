#include "event_aggregator.h"

#include <limits.h>
#include <string.h>

static void IncrementSaturatingU16(uint16_t *value)
{
    if ((value != NULL) && (*value < UINT16_MAX))
    {
        (*value)++;
    }
}

static uint8_t RingPreviousIndex(uint8_t head, uint8_t capacity)
{
    return (head == 0U) ? (uint8_t)(capacity - 1U) : (uint8_t)(head - 1U);
}

static bool FindTemperatureLookback(
    const event_aggregator_t *aggregator,
    uint32_t current_timestamp_s,
    uint32_t lookback_s,
    int32_t *temperature_mdeg_c)
{
    uint8_t index;
    uint8_t i;
    uint32_t best_error_s = UINT32_MAX;
    bool found = false;

    if ((aggregator == NULL) || (temperature_mdeg_c == NULL) ||
        (current_timestamp_s < lookback_s) ||
        (aggregator->temperature_history_count == 0U))
    {
        return false;
    }

    index = RingPreviousIndex(
        aggregator->temperature_history_head,
        EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY);

    for (i = 0U; i < aggregator->temperature_history_count; i++)
    {
        const event_aggregator_temperature_point_t *point =
            &aggregator->temperature_history[index];

        if (point->timestamp_s <= current_timestamp_s)
        {
            uint32_t age_s = current_timestamp_s - point->timestamp_s;
            uint32_t error_s = (age_s >= lookback_s)
                                   ? (age_s - lookback_s)
                                   : (lookback_s - age_s);

            if ((error_s <= EVENT_AGGREGATOR_LOOKBACK_TOLERANCE_S) &&
                (error_s < best_error_s))
            {
                *temperature_mdeg_c = point->temperature_mdeg_c;
                best_error_s = error_s;
                found = true;

                if (error_s == 0U)
                {
                    break;
                }
            }
        }

        index = (index == 0U)
                    ? (uint8_t)(EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY - 1U)
                    : (uint8_t)(index - 1U);
    }

    return found;
}

static void StoreTemperatureHistory(
    event_aggregator_t *aggregator,
    uint32_t timestamp_s,
    int32_t temperature_mdeg_c)
{
    uint8_t latest_index;

    if (aggregator->temperature_history_count > 0U)
    {
        latest_index = RingPreviousIndex(
            aggregator->temperature_history_head,
            EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY);

        if (aggregator->temperature_history[latest_index].timestamp_s == timestamp_s)
        {
            aggregator->temperature_history[latest_index].temperature_mdeg_c =
                temperature_mdeg_c;
            return;
        }
    }

    aggregator->temperature_history[aggregator->temperature_history_head].timestamp_s =
        timestamp_s;
    aggregator->temperature_history[aggregator->temperature_history_head].temperature_mdeg_c =
        temperature_mdeg_c;

    aggregator->temperature_history_head =
        (uint8_t)((aggregator->temperature_history_head + 1U) %
                  EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY);

    if (aggregator->temperature_history_count <
        EVENT_AGGREGATOR_TEMP_HISTORY_CAPACITY)
    {
        aggregator->temperature_history_count++;
    }
}

static void UpdateTemperatureSummary(
    event_aggregator_t *aggregator,
    int32_t temperature_mdeg_c)
{
    event_aggregator_window_summary_t *summary = &aggregator->summary;

    if (!summary->temperature_valid)
    {
        summary->temperature_valid = true;
        summary->temperature_current_mdeg_c = temperature_mdeg_c;
        summary->temperature_min_mdeg_c = temperature_mdeg_c;
        summary->temperature_max_mdeg_c = temperature_mdeg_c;
        summary->max_negative_excursion_mdeg_c = 0;
    }
    else
    {
        int32_t excursion_from_prior_max =
            temperature_mdeg_c - summary->temperature_max_mdeg_c;

        if (excursion_from_prior_max < summary->max_negative_excursion_mdeg_c)
        {
            summary->max_negative_excursion_mdeg_c = excursion_from_prior_max;
        }

        if (temperature_mdeg_c < summary->temperature_min_mdeg_c)
        {
            summary->temperature_min_mdeg_c = temperature_mdeg_c;
        }

        if (temperature_mdeg_c > summary->temperature_max_mdeg_c)
        {
            summary->temperature_max_mdeg_c = temperature_mdeg_c;
        }

        summary->temperature_current_mdeg_c = temperature_mdeg_c;
    }

    IncrementSaturatingU16(&summary->temperature_sample_count);
}

static void UpdateIntervalSummary(
    event_aggregator_t *aggregator,
    uint16_t interval_s)
{
    event_aggregator_window_summary_t *summary = &aggregator->summary;
    uint64_t mean_s;
    uint64_t mean_square_s2;
    uint64_t variance_s2;

    aggregator->inter_event_interval_sum_s += interval_s;
    aggregator->inter_event_interval_sum_sq_s2 +=
        ((uint64_t)interval_s * (uint64_t)interval_s);

    if (!summary->inter_event_interval_valid)
    {
        summary->inter_event_interval_valid = true;
        summary->inter_event_interval_min_s = interval_s;
        summary->inter_event_interval_max_s = interval_s;
    }
    else
    {
        if (interval_s < summary->inter_event_interval_min_s)
        {
            summary->inter_event_interval_min_s = interval_s;
        }

        if (interval_s > summary->inter_event_interval_max_s)
        {
            summary->inter_event_interval_max_s = interval_s;
        }
    }

    IncrementSaturatingU16(&summary->inter_event_interval_count);

    if (summary->inter_event_interval_count == 0U)
    {
        return;
    }

    mean_s = aggregator->inter_event_interval_sum_s /
             summary->inter_event_interval_count;
    mean_square_s2 = aggregator->inter_event_interval_sum_sq_s2 /
                     summary->inter_event_interval_count;

    variance_s2 = (mean_square_s2 >= (mean_s * mean_s))
                      ? (mean_square_s2 - (mean_s * mean_s))
                      : 0U;

    summary->inter_event_interval_mean_s =
        (mean_s > UINT16_MAX) ? UINT16_MAX : (uint16_t)mean_s;
    summary->inter_event_interval_variance_s2 =
        (variance_s2 > UINT32_MAX) ? UINT32_MAX : (uint32_t)variance_s2;
}

static void StoreEventRecord(
    event_aggregator_t *aggregator,
    const event_processor_features_t *features,
    const event_processor_result_t *result)
{
    event_aggregator_event_record_t *record =
        &aggregator->event_records[aggregator->event_record_head];

    memset(record, 0, sizeof(*record));

    record->timestamp_s = features->timestamp_s;
    record->flags = result->flags;
    record->motion_duration_ms = features->motion_duration_ms;
    record->inter_event_interval_valid = features->inter_event_interval_valid;
    record->inter_event_interval_s = features->inter_event_interval_s;
    record->temperature_valid = features->temperature_valid;
    record->temperature_mdeg_c = features->temperature_mdeg_c;
    record->peak_angular_velocity_dps = features->peak_angular_velocity_dps;
    record->total_orientation_change_cdeg =
        features->total_orientation_change_cdeg;

    aggregator->event_record_head =
        (uint8_t)((aggregator->event_record_head + 1U) %
                  EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY);

    if (aggregator->event_record_count < EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY)
    {
        aggregator->event_record_count++;
    }
    else
    {
        IncrementSaturatingU16(
            &aggregator->summary.overwritten_event_record_count);
    }

    aggregator->summary.retained_event_record_count =
        aggregator->event_record_count;
}

static void AggregateProcessorResult(
    event_aggregator_t *aggregator,
    const event_processor_result_t *result)
{
    event_aggregator_window_summary_t *summary = &aggregator->summary;

    summary->combined_flags |= result->flags;

    if ((result->flags & EVENT_PROCESSOR_FLAG_TEMP_DROP_REFERENCE) != 0U)
    {
        IncrementSaturatingU16(&summary->temp_drop_reference_count);
    }

    if ((result->flags & EVENT_PROCESSOR_FLAG_CONTRACTION_CANDIDATE) != 0U)
    {
        IncrementSaturatingU16(&summary->contraction_candidate_count);
    }

    if ((result->flags & EVENT_PROCESSOR_FLAG_CONTRACTION_PERIODICITY) != 0U)
    {
        IncrementSaturatingU16(&summary->contraction_periodicity_count);
    }

    if ((result->flags & EVENT_PROCESSOR_FLAG_HYPERTHERMIA_REFERENCE) != 0U)
    {
        IncrementSaturatingU16(&summary->hyperthermia_reference_count);
    }

    if ((result->flags & EVENT_PROCESSOR_FLAG_ROTATION_CANDIDATE) != 0U)
    {
        IncrementSaturatingU16(&summary->rotation_candidate_count);
    }
}

event_aggregator_status_t EventAggregator_Init(
    event_aggregator_t *aggregator,
    const bolus_runtime_config_t *config,
    uint32_t window_start_s)
{
    if ((aggregator == NULL) || (config == NULL))
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_AGGREGATOR_ERROR_CONFIG;
    }

    memset(aggregator, 0, sizeof(*aggregator));

    aggregator->initialized = true;
    aggregator->window_period_s = config->radio.uplink_period_s;
    aggregator->summary.window_start_s = window_start_s;
    aggregator->summary.window_end_s = window_start_s;
    aggregator->summary.window_period_s = config->radio.uplink_period_s;

    return EVENT_AGGREGATOR_OK;
}

event_aggregator_status_t EventAggregator_ProcessObservation(
    event_aggregator_t *aggregator,
    const bolus_runtime_config_t *config,
    const event_aggregator_observation_t *observation,
    event_processor_features_t *features,
    event_processor_result_t *result)
{
    int32_t reference_temperature_mdeg_c;
    uint32_t interval_s;
    event_processor_status_t processor_status;

    if ((aggregator == NULL) || (config == NULL) || (observation == NULL) ||
        (features == NULL) || (result == NULL))
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    if (!aggregator->initialized)
    {
        return EVENT_AGGREGATOR_ERROR_NOT_INITIALIZED;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_AGGREGATOR_ERROR_CONFIG;
    }

    if (aggregator->last_observation_timestamp_valid &&
        (observation->timestamp_s < aggregator->last_observation_timestamp_s))
    {
        return EVENT_AGGREGATOR_ERROR_TIME;
    }

    aggregator->window_period_s = config->radio.uplink_period_s;
    aggregator->summary.window_period_s = config->radio.uplink_period_s;
    aggregator->summary.window_end_s = observation->timestamp_s;
    aggregator->last_observation_timestamp_s = observation->timestamp_s;
    aggregator->last_observation_timestamp_valid = true;

    memset(features, 0, sizeof(*features));

    features->timestamp_s = observation->timestamp_s;
    features->motion_event = observation->motion_event;
    features->motion_duration_ms = observation->motion_duration_ms;
    features->rotation_candidate = observation->rotation_candidate;
    features->peak_angular_velocity_dps =
        observation->peak_angular_velocity_dps;
    features->total_orientation_change_cdeg =
        observation->total_orientation_change_cdeg;

    if (observation->temperature_valid)
    {
        StoreTemperatureHistory(
            aggregator,
            observation->timestamp_s,
            observation->temperature_mdeg_c);
        UpdateTemperatureSummary(aggregator, observation->temperature_mdeg_c);

        features->temperature_valid = true;
        features->temperature_mdeg_c = observation->temperature_mdeg_c;

        if (FindTemperatureLookback(
                aggregator,
                observation->timestamp_s,
                300U,
                &reference_temperature_mdeg_c))
        {
            features->temperature_delta_5min_valid = true;
            features->temperature_delta_5min_mdeg_c =
                observation->temperature_mdeg_c - reference_temperature_mdeg_c;
        }

        if (FindTemperatureLookback(
                aggregator,
                observation->timestamp_s,
                600U,
                &reference_temperature_mdeg_c))
        {
            features->temperature_delta_10min_valid = true;
            features->temperature_delta_10min_mdeg_c =
                observation->temperature_mdeg_c - reference_temperature_mdeg_c;
        }
    }

    if (observation->motion_event)
    {
        IncrementSaturatingU16(&aggregator->summary.motion_event_count);

        if (aggregator->last_motion_timestamp_valid &&
            (observation->timestamp_s >= aggregator->last_motion_timestamp_s))
        {
            interval_s =
                observation->timestamp_s - aggregator->last_motion_timestamp_s;

            if ((interval_s > 0U) && (interval_s <= UINT16_MAX))
            {
                features->inter_event_interval_valid = true;
                features->inter_event_interval_s = (uint16_t)interval_s;
                UpdateIntervalSummary(
                    aggregator,
                    features->inter_event_interval_s);
            }
        }

        aggregator->last_motion_timestamp_s = observation->timestamp_s;
        aggregator->last_motion_timestamp_valid = true;
    }

    processor_status = EventProcessor_Evaluate(config, features, result);
    if (processor_status != EVENT_PROCESSOR_OK)
    {
        return EVENT_AGGREGATOR_ERROR_PROCESSOR;
    }

    AggregateProcessorResult(aggregator, result);

    if (observation->motion_event || (result->flags != 0U))
    {
        StoreEventRecord(aggregator, features, result);
    }

    return EVENT_AGGREGATOR_OK;
}

bool EventAggregator_IsWindowDue(
    const event_aggregator_t *aggregator,
    uint32_t now_s)
{
    if ((aggregator == NULL) || (!aggregator->initialized) ||
        (now_s < aggregator->summary.window_start_s))
    {
        return false;
    }

    return ((now_s - aggregator->summary.window_start_s) >=
            aggregator->window_period_s);
}

event_aggregator_status_t EventAggregator_GetWindowSummary(
    const event_aggregator_t *aggregator,
    event_aggregator_window_summary_t *summary)
{
    if ((aggregator == NULL) || (summary == NULL))
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    if (!aggregator->initialized)
    {
        return EVENT_AGGREGATOR_ERROR_NOT_INITIALIZED;
    }

    *summary = aggregator->summary;
    return EVENT_AGGREGATOR_OK;
}

event_aggregator_status_t EventAggregator_GetEventRecord(
    const event_aggregator_t *aggregator,
    uint16_t index,
    event_aggregator_event_record_t *record)
{
    uint8_t oldest_index;
    uint8_t record_index;

    if ((aggregator == NULL) || (record == NULL))
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    if (!aggregator->initialized)
    {
        return EVENT_AGGREGATOR_ERROR_NOT_INITIALIZED;
    }

    if (index >= aggregator->event_record_count)
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    oldest_index =
        (uint8_t)((aggregator->event_record_head +
                   EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY -
                   aggregator->event_record_count) %
                  EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY);

    record_index =
        (uint8_t)((oldest_index + index) %
                  EVENT_AGGREGATOR_EVENT_RECORD_CAPACITY);

    *record = aggregator->event_records[record_index];
    return EVENT_AGGREGATOR_OK;
}

event_aggregator_status_t EventAggregator_ResetWindow(
    event_aggregator_t *aggregator,
    uint32_t new_window_start_s)
{
    if (aggregator == NULL)
    {
        return EVENT_AGGREGATOR_ERROR_PARAM;
    }

    if (!aggregator->initialized)
    {
        return EVENT_AGGREGATOR_ERROR_NOT_INITIALIZED;
    }

    if (aggregator->last_observation_timestamp_valid &&
        (new_window_start_s < aggregator->last_observation_timestamp_s))
    {
        return EVENT_AGGREGATOR_ERROR_TIME;
    }

    memset(&aggregator->summary, 0, sizeof(aggregator->summary));
    memset(aggregator->event_records, 0, sizeof(aggregator->event_records));

    aggregator->event_record_head = 0U;
    aggregator->event_record_count = 0U;
    aggregator->inter_event_interval_sum_s = 0U;
    aggregator->inter_event_interval_sum_sq_s2 = 0U;

    aggregator->summary.window_start_s = new_window_start_s;
    aggregator->summary.window_end_s = new_window_start_s;
    aggregator->summary.window_period_s = aggregator->window_period_s;

    return EVENT_AGGREGATOR_OK;
}
