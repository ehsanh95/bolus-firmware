#ifndef TELEMETRY_CODEC_H
#define TELEMETRY_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include "../Application/telemetry_data.h"

#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V1       1U
#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V2       2U
#define BOLUS_TELEMETRY_PROTOCOL_VERSION_V2_1     3U
#define BOLUS_TELEMETRY_MESSAGE_TYPE_SUMMARY      1U
#define BOLUS_TELEMETRY_SUMMARY_V1_SIZE           24U
#define BOLUS_TELEMETRY_SUMMARY_V2_SIZE           32U
#define BOLUS_TELEMETRY_SUMMARY_V2_1_SIZE         42U

/* Legacy V1 status bits. */
#define BOLUS_TELEMETRY_STATUS_TEMP_VALID         (1U << 0)
#define BOLUS_TELEMETRY_STATUS_MOTION_VALID       (1U << 1)
#define BOLUS_TELEMETRY_STATUS_INTERVAL_VALID     (1U << 2)
#define BOLUS_TELEMETRY_STATUS_EVENT_OVERFLOW     (1U << 3)
#define BOLUS_TELEMETRY_STATUS_FAULT_PRESENT      (1U << 4)
#define BOLUS_TELEMETRY_STATUS_HEALTH_DEGRADED    (1U << 5)
#define BOLUS_TELEMETRY_STATUS_HEALTH_CRITICAL    (1U << 6)

/* Episode-aware V2/V2.1 status bits. */
#define BOLUS_TELEMETRY_V2_STATUS_TEMP_VALID       (1U << 0)
#define BOLUS_TELEMETRY_V2_STATUS_MOTION_VALID     (1U << 1)
#define BOLUS_TELEMETRY_V2_STATUS_INTERVAL_VALID   (1U << 2)
#define BOLUS_TELEMETRY_V2_STATUS_MPU_VALID        (1U << 3)
#define BOLUS_TELEMETRY_V2_STATUS_FAULT_PRESENT    (1U << 4)
#define BOLUS_TELEMETRY_V2_STATUS_HEALTH_DEGRADED  (1U << 5)
#define BOLUS_TELEMETRY_V2_STATUS_HEALTH_CRITICAL  (1U << 6)
#define BOLUS_TELEMETRY_V2_STATUS_STAGING_UNTESTED (1U << 7)

typedef enum
{
    TELEMETRY_CODEC_OK = 0,
    TELEMETRY_CODEC_ERROR_PARAM,
    TELEMETRY_CODEC_ERROR_BUFFER
} telemetry_codec_status_t;

/* Legacy fixed 24-byte summary, retained for compatibility. */
telemetry_codec_status_t TelemetryCodec_EncodeSummaryV1(
    const bolus_telemetry_summary_v1_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

/*
 * Episode-aware compact V2 wire layout, 32 bytes total:
 *   0      protocol-version[7:4] | message-type[3:0]
 *   1..2   sequence
 *   3      config version, saturated to 255
 *   4      V2 status/validity bitmap
 *   5      battery percent
 *   6..7   battery mV
 *   8..9   current temperature, centi-C
 *   10..11 minimum temperature, centi-C
 *   12..13 maximum temperature, centi-C
 *   14..15 maximum negative temperature excursion, centi-C signed
 *   16     episodes started in the 15-minute window
 *   17     accepted motion pulses
 *   18     pulses suppressed by retrigger guard
 *   19     maximum accepted pulses observed in one episode
 *   20     mean inter-pulse interval, seconds
 *   21     inter-pulse interval standard deviation, seconds
 *   22     successful MPU burst count
 *   23     mean MPU dynamic-acceleration RMS / 20 mg
 *   24     MPU peak dynamic acceleration / 20 mg
 *   25     mean MPU angular-velocity RMS / 10 dps
 *   26     MPU peak angular velocity / 10 dps
 *   27     max orientation change / 2 degrees
 *   28     total angular motion / 5 degrees
 *   29     contraction-candidate count (reserved until classifier connected)
 *   30     rotation-candidate count (reserved until classifier connected)
 *   31     low 8 bits of combined event/reference flags
 *
 * This payload intentionally transmits compact features, never raw waveforms.
 */
telemetry_codec_status_t TelemetryCodec_EncodeSummaryV2(
    const bolus_telemetry_summary_v2_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

/*
 * Telemetry V2.1 wire layout, 42 bytes total.
 *
 * Bytes 0..31 retain the complete V2 layout above. Only byte 0 changes to
 * 0x31 so a decoder can dispatch the packet to V2.1 without length guessing.
 * Native BMA456 data is appended without changing or removing any V2 field:
 *   32..35 BMA456 Step Counter, unsigned little-endian
 *   36..37 BMA456 acceleration X, signed mg little-endian
 *   38..39 BMA456 acceleration Y, signed mg little-endian
 *   40..41 BMA456 acceleration Z, signed mg little-endian
 *
 * The BMA values are full-resolution application units and must never be
 * sourced from MPU6050 burst samples.
 */
telemetry_codec_status_t TelemetryCodec_EncodeSummaryV2_1(
    const bolus_telemetry_summary_v2_1_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size);

#endif /* TELEMETRY_CODEC_H */
