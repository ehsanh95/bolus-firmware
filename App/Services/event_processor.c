#include "event_processor.h"

#include <string.h>

static bool IsWithinU16(uint16_t value, uint16_t minimum, uint16_t maximum)
{
    return ((value >= minimum) && (value <= maximum));
}

event_processor_status_t EventProcessor_Evaluate(
    const bolus_runtime_config_t *config,
    const event_processor_features_t *features,
    event_processor_result_t *result)
{
    int32_t drop_5min_threshold;
    int32_t drop_10min_threshold;

    if ((config == NULL) || (features == NULL) || (result == NULL))
    {
        return EVENT_PROCESSOR_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_PROCESSOR_ERROR_CONFIG;
    }

    memset(result, 0, sizeof(*result));
    result->timestamp_s = features->timestamp_s;
    result->rule_source = config->event_processing.rule_source;

    if (!config->event_processing.enable)
    {
        return EVENT_PROCESSOR_OK;
    }

    if (features->motion_event)
    {
        result->flags |= EVENT_PROCESSOR_FLAG_GENERAL_MOTION;
    }

    /*
     * Published drinking algorithms provide 0.5 C / 5 min and 0.5 C / 10 min
     * fall scales. These are the preferred literature-reference matches.
     * High-rate TMP117 thresholds must be field-calibrated before production.
     */
    drop_5min_threshold =
        -(int32_t)config->event_processing.drinking_drop_5min_mdeg_c;
    drop_10min_threshold =
        -(int32_t)config->event_processing.drinking_drop_10min_mdeg_c;

    if (features->temperature_valid)
    {
        if (features->temperature_delta_5min_valid &&
            (features->temperature_delta_5min_mdeg_c <= drop_5min_threshold))
        {
            result->drinking_5min_reference_match = true;
        }

        if (features->temperature_delta_10min_valid &&
            (features->temperature_delta_10min_mdeg_c <= drop_10min_threshold))
        {
            result->drinking_10min_reference_match = true;
        }

        if (result->drinking_5min_reference_match ||
            result->drinking_10min_reference_match)
        {
            result->flags |= EVENT_PROCESSOR_FLAG_TEMP_DROP_REFERENCE;
        }

        /*
         * A fixed 38.1 C rule exists in the published drinking literature, but
         * it performed worse than trajectory-based methods and is therefore
         * retained only as an independent secondary reference flag.
         */
        if (features->temperature_mdeg_c <=
            config->event_processing.drinking_absolute_temp_reference_mdeg_c)
        {
            result->drinking_absolute_temp_reference_match = true;
            result->flags |= EVENT_PROCESSOR_FLAG_DRINK_ABS_TEMP_REFERENCE;
        }

        /*
         * These are health-risk/reference flags only. They intentionally do not
         * produce fever/infection/SARA diagnoses.
         */
        if (features->temperature_mdeg_c >=
            config->event_processing.hyperthermia_reference_mdeg_c)
        {
            result->hyperthermia_reference_match = true;
            result->flags |= EVENT_PROCESSOR_FLAG_HYPERTHERMIA_REFERENCE;
        }

        if (features->temperature_mdeg_c >=
            config->event_processing.sara_risk_reference_mdeg_c)
        {
            result->sara_risk_reference_match = true;
            result->flags |= EVENT_PROCESSOR_FLAG_SARA_RISK_REFERENCE;
        }
    }

    /*
     * Direct intrareticular studies show an approximately 8-10 s contraction
     * morphology and roughly 40-60 s recurrence. Duration is kept separate
     * from periodicity so the temporal aggregator can accumulate evidence
     * rather than forcing one isolated event to carry a behavioral label.
     */
    if (features->motion_event &&
        IsWithinU16(
            features->motion_duration_ms,
            config->event_processing.contraction_duration_min_ms,
            config->event_processing.contraction_duration_max_ms))
    {
        result->contraction_duration_match = true;
        result->flags |= EVENT_PROCESSOR_FLAG_CONTRACTION_CANDIDATE;
    }

    if (result->contraction_duration_match &&
        features->inter_event_interval_valid &&
        IsWithinU16(
            features->inter_event_interval_s,
            config->event_processing.contraction_interval_min_s,
            config->event_processing.contraction_interval_max_s))
    {
        result->contraction_interval_match = true;
        result->flags |= EVENT_PROCESSOR_FLAG_CONTRACTION_PERIODICITY;
    }

    /*
     * No universal intraruminal gyro threshold is encoded here. A future
     * calibrated/adaptive rotation detector supplies rotation_candidate while
     * raw gyro features remain available for field-trial feature ablation.
     */
    if (features->rotation_candidate)
    {
        result->flags |= EVENT_PROCESSOR_FLAG_ROTATION_CANDIDATE;
    }

    return EVENT_PROCESSOR_OK;
}
