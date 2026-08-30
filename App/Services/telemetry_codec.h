#ifndef TELEMETRY_CODEC_H
#define TELEMETRY_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "../Application/telemetry_data.h"

#define BOLUS_TELEMETRY_PROTOCOL_VERSION          1U
#define BOLUS_TELEMETRY_MESSAGE_TYPE_SUMMARY      1U
#define BOLUS_TELEMETRY_SUMMARY_V1_SIZE           24U

#define BOLUS_TELEMETRY_STATUS_TEMP_VALID         (1U << 0)
#define BOLUS_TELEMETRY_STATUS_MOTION_VALID       (1U << 1)
#define BOLUS_TELEMETRY_STATUS_INTERVAL_VALID     (1U << 2)
#define BOLUS_TELEMETRY_STATUS_EVENT_OVERFLOW     (1U << 3)
#define BOLUS_TELEMETRY_STATUS_FAULT_PRESENT      (1U << 4)
#define BOLUS_TELEMETRY_STATUS_HEALTH_DEGRADED    (1U << 5)
#define BOLUS_TELEMETRY_STATUS_HEALTH_CRITICAL    (1U << 6)

typedef enum
{
    TELEMETRY_CODEC_OK = 0,
    TELEMETRY_CODEC_ERROR_PARAM,
    TELEMETRY_CODEC_ERROR_BUFFER
} telemetry_codec_status_t;

/*
 * Encode the fixed 24-byte summary payload explicitly; never memcpy an
 * application struct onto the radio.
 *
 * Wire layout (little-endian multibyte fields):
 *   0      protocol-version[7:4] | message-type[3:0]
 *   1..2   sequence
 *   3      config version, saturated to 255
 *   4      status/validity bitmap
 *   5      battery percent
 *   6..7   battery mV
 *   8..9   current temperature, centi-C
 *   10..11 minimum temperature, centi-C
 *   12..13 maximum temperature, centi-C
 *   14..15 maximum negative excursion, centi-C signed
 *   16     motion event count, saturated
 *   17     contraction-candidate count, saturated
 *   18     rotation-candidate count, saturated
 *   19     mean inter-event interval, seconds, saturated
 *   20     inter-event interval standard deviation, seconds, saturated
 *   21     RMS dynamic acceleration / 20 mg, saturated
 *   22     peak dynamic acceleration / 20 mg, saturated
 *   23     low 8 bits of combined physical/reference event flags
 */
telemetry_codec_status_t TelemetryCodec_EncodeSummaryV1(
    const bolus_telemetry_summary_v1_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

#endif /* TELEMETRY_CODEC_H */
