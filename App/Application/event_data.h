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

#define BOLUS_EVENT_TEMP_POINT_COUNT              4U
#define BOLUS_EVENT_TEMP_T0_OFFSET_MS             0U
#define BOLUS_EVENT_TEMP_T5_OFFSET_MS             5000U
#define BOLUS_EVENT_TEMP_T10_OFFSET_MS            10000U
#define BOLUS_EVENT_TEMP_T20_OFFSET_MS            20000U

typedef enum
{
    BOLUS_EVENT_TEMP_T0 = 0,
    BOLUS_EVENT_TEMP_T5,
    BOLUS_EVENT_TEMP_T10,
    BOLUS_EVENT_TEMP_T20
} bolus_event_temperature_point_t;

#define BOLUS_EVENT_TEMP_VALID_T0   (1U << BOLUS_EVENT_TEMP_T0)
#define BOLUS_EVENT_TEMP_VALID_T5   (1U << BOLUS_EVENT_TEMP_T5)
#define BOLUS_EVENT_TEMP_VALID_T10  (1U << BOLUS_EVENT_TEMP_T10)
#define BOLUS_EVENT_TEMP_VALID_T20  (1U << BOLUS_EVENT_TEMP_T20)

/*
 * Compact per-event feature record retained locally before the 15-minute
 * telemetry aggregation step.
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
