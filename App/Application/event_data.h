#ifndef EVENT_DATA_H
#define EVENT_DATA_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Application-level event data contracts.
 *
 * These structures are NOT the LoRa/LoRaWAN wire format and must never be
 * memcpy'd directly into a radio payload. The radio encoder is responsible for
 * explicit scaling, field order and protocol versioning.
 */

#define BOLUS_EVENT_MOTION_CAPTURE_TARGET_MS      2000U

/*
 * First-pulse thermal trajectory points. If pulse #2 arrives in the same Event
 * Episode, the remaining scheduled points are cancelled and later pulse
 * records use their immediate T0 temperature instead.
 */
#define BOLUS_EVENT_TEMP_POINT_COUNT              5U
#define BOLUS_EVENT_TEMP_T0_OFFSET_MS             0U
#define BOLUS_EVENT_TEMP_T5_OFFSET_MS             5000U
#define BOLUS_EVENT_TEMP_T15_OFFSET_MS            15000U
#define BOLUS_EVENT_TEMP_T35_OFFSET_MS            35000U
#define BOLUS_EVENT_TEMP_T65_OFFSET_MS            65000U

typedef enum
{
    BOLUS_EVENT_TEMP_T0 = 0,
    BOLUS_EVENT_TEMP_T5,
    BOLUS_EVENT_TEMP_T15,
    BOLUS_EVENT_TEMP_T35,
    BOLUS_EVENT_TEMP_T65
} bolus_event_temperature_point_t;

#define BOLUS_EVENT_TEMP_VALID_T0   (1U << BOLUS_EVENT_TEMP_T0)
#define BOLUS_EVENT_TEMP_VALID_T5   (1U << BOLUS_EVENT_TEMP_T5)
#define BOLUS_EVENT_TEMP_VALID_T15  (1U << BOLUS_EVENT_TEMP_T15)
#define BOLUS_EVENT_TEMP_VALID_T35  (1U << BOLUS_EVENT_TEMP_T35)
#define BOLUS_EVENT_TEMP_VALID_T65  (1U << BOLUS_EVENT_TEMP_T65)

/*
 * Compact per-motion-pulse feature record retained locally before the
 * 15-minute telemetry aggregation step. Several pulse records may belong to
 * one higher-level Event Episode.
 *
 * Motion features are orientation-independent or relative features. Raw XYZ
 * samples are intentionally not retained in normal mode.
 *
 * peak_dynamic_accel_mg / rms_dynamic_accel_mg:
 *   magnitude of |a|-1g during the short event-capture window.
 *
 * accel_variance_mg2:
 *   variance of the dynamic acceleration magnitude.
 *
 * peak_jerk_mg_s / rms_jerk_mg_s:
 *   resultant delta-acceleration divided by the measured sample interval.
 */
typedef struct
{
    uint32_t event_sequence;
    uint32_t timestamp_s;

    uint16_t capture_duration_ms;
    uint16_t sample_count;

    uint16_t peak_dynamic_accel_mg;
    uint16_t rms_dynamic_accel_mg;
    uint32_t accel_variance_mg2;

    uint32_t peak_jerk_mg_s;
    uint32_t rms_jerk_mg_s;

    bool inter_event_interval_valid;
    uint16_t inter_event_interval_s;

    uint8_t temperature_valid_mask;
    int32_t temperature_mdeg_c[BOLUS_EVENT_TEMP_POINT_COUNT];

    /* Reserved for later MPU/processor stages; zero means not available. */
    bool rotation_features_valid;
    uint16_t peak_angular_velocity_dps;
    uint16_t total_orientation_change_cdeg;

    uint32_t processor_flags;
} bolus_event_feature_record_t;

#endif /* EVENT_DATA_H */
