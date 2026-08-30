#ifndef EVENT_FEATURE_EXTRACTOR_H
#define EVENT_FEATURE_EXTRACTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "../Application/event_data.h"

typedef enum
{
    EVENT_FEATURE_EXTRACTOR_OK = 0,
    EVENT_FEATURE_EXTRACTOR_ERROR_PARAM,
    EVENT_FEATURE_EXTRACTOR_ERROR_STATE,
    EVENT_FEATURE_EXTRACTOR_NOT_COMPLETE
} event_feature_extractor_status_t;

typedef struct
{
    bool active;
    uint32_t event_sequence;
    uint32_t event_timestamp_s;
    uint32_t start_tick_ms;
    uint32_t last_tick_ms;

    bool previous_sample_valid;
    int16_t previous_x_mg;
    int16_t previous_y_mg;
    int16_t previous_z_mg;

    uint16_t sample_count;
    uint16_t peak_dynamic_accel_mg;
    uint32_t peak_jerk_mg_s;

    uint64_t dynamic_sum_mg;
    uint64_t dynamic_sum_sq_mg2;
    uint64_t jerk_sum_sq_mg2_s2;
    uint16_t jerk_sample_count;
} event_feature_extractor_t;

/* Start a new short motion-capture window after an accepted BMA event. */
event_feature_extractor_status_t EventFeatureExtractor_Start(
    event_feature_extractor_t *extractor,
    uint32_t event_sequence,
    uint32_t event_timestamp_s,
    uint32_t start_tick_ms);

/* Add one BMA XYZ sample. No raw sample history is retained. */
event_feature_extractor_status_t EventFeatureExtractor_AddBmaSample(
    event_feature_extractor_t *extractor,
    int16_t x_mg,
    int16_t y_mg,
    int16_t z_mg,
    uint32_t sample_tick_ms);

/* True once the configured V1 short capture duration has elapsed. */
bool EventFeatureExtractor_IsMotionWindowComplete(
    const event_feature_extractor_t *extractor,
    uint32_t now_tick_ms);

/*
 * Finish the short motion window and emit the compact event record.
 * Temperature follow-up points can be attached later without retaining the
 * raw BMA waveform.
 */
event_feature_extractor_status_t EventFeatureExtractor_FinalizeMotion(
    event_feature_extractor_t *extractor,
    uint32_t now_tick_ms,
    bolus_event_feature_record_t *record);

/* Attach one sparse TMP117 point to an already-created event record. */
event_feature_extractor_status_t EventFeatureRecord_SetTemperature(
    bolus_event_feature_record_t *record,
    bolus_event_temperature_point_t point,
    int32_t temperature_mdeg_c);

#endif /* EVENT_FEATURE_EXTRACTOR_H */
