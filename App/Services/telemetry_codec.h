#ifndef TELEMETRY_CODEC_H
#define TELEMETRY_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "../Application/telemetry_data.h"

#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V1       1U
#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V2       2U
#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V21      3U
#define BOLUS_TELEMETRY_MESSAGE_TYPE_SUMMARY      1U
#define BOLUS_TELEMETRY_SUMMARY_V1_SIZE           24U
#define BOLUS_TELEMETRY_SUMMARY_V2_SIZE           32U
#define BOLUS_TELEMETRY_SUMMARY_V21_SIZE          40U

#define BOLUS_TELEMETRY_STATUS_TEMP_VALID         (1U << 0)
#define BOLUS_TELEMETRY_STATUS_MOTION_VALID       (1U << 1)
#define BOLUS_TELEMETRY_STATUS_INTERVAL_VALID     (1U << 2)
#define BOLUS_TELEMETRY_STATUS_EVENT_OVERFLOW     (1U << 3)
#define BOLUS_TELEMETRY_STATUS_FAULT_PRESENT      (1U << 4)
#define BOLUS_TELEMETRY_STATUS_HEALTH_DEGRADED    (1U << 5)
#define BOLUS_TELEMETRY_STATUS_HEALTH_CRITICAL    (1U << 6)

#define BOLUS_TELEMETRY_V2_STATUS_TEMP_VALID       (1U << 0)
#define BOLUS_TELEMETRY_V2_STATUS_MOTION_VALID     (1U << 1)
#define BOLUS_TELEMETRY_V2_STATUS_INTERVAL_VALID   (1U << 2)
#define BOLUS_TELEMETRY_V2_STATUS_MPU_VALID        (1U << 3)
#define BOLUS_TELEMETRY_V2_STATUS_FAULT_PRESENT    (1U << 4)
#define BOLUS_TELEMETRY_V2_STATUS_HEALTH_DEGRADED  (1U << 5)
#define BOLUS_TELEMETRY_V2_STATUS_HEALTH_CRITICAL  (1U << 6)
#define BOLUS_TELEMETRY_V2_STATUS_STAGING_UNTESTED (1U << 7)

#define BOLUS_TELEMETRY_V21_STATUS_BMA456_VALID    (1U << 7)

typedef enum
{
    TELEMETRY_CODEC_OK = 0,
    TELEMETRY_CODEC_ERROR_PARAM,
    TELEMETRY_CODEC_ERROR_BUFFER
} telemetry_codec_status_t;

telemetry_codec_status_t TelemetryCodec_EncodeSummaryV1(
    const bolus_telemetry_summary_v1_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

telemetry_codec_status_t TelemetryCodec_EncodeSummaryV2(
    const bolus_telemetry_summary_v2_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

/*
 * Extended payload preserving all V2 fields and appending native BMA456 data.
 * No MPU6050 data is used for these fields.
 *
 * Additional bytes:
 * 32..35 bma456 step counter
 * 36..37 bma456 accel X mg
 * 38..39 bma456 accel Y mg
 * 40..41 bma456 accel Z mg
 */
telemetry_codec_status_t TelemetryCodec_EncodeSummaryV21(
    const bolus_telemetry_summary_v2_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

#endif /* TELEMETRY_CODEC_H */
