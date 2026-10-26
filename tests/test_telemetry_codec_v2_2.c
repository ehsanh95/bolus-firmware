#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "telemetry_codec.h"

static bolus_telemetry_summary_v2_2_t BuildSummary(void)
{
    bolus_telemetry_summary_v2_2_t summary = {0};

    summary.v2.sequence = 0x0010U;
    summary.v2.config_version = 11U;
    summary.v2.temperature_valid = true;
    summary.v2.temperature_current_mdeg_c = 24450;
    summary.v2.temperature_min_mdeg_c = 24440;
    summary.v2.temperature_max_mdeg_c = 24520;
    summary.v2.max_negative_excursion_mdeg_c = -60;
    summary.v2.motion_valid = true;
    summary.v2.episode_count = 1U;
    summary.v2.accepted_pulse_count = 3U;
    summary.v2.suppressed_pulse_count = 8U;
    summary.v2.max_pulses_per_episode = 3U;
    summary.v2.inter_pulse_interval_valid = true;
    summary.v2.inter_pulse_interval_mean_s = 2U;
    summary.v2.inter_pulse_interval_variance_s2 = 0U;
    summary.v2.mpu_valid = true;
    summary.v2.mpu_burst_count = 3U;
    summary.v2.rms_dynamic_accel_mg = 1180U;
    summary.v2.peak_dynamic_accel_mg = 1900U;
    summary.v2.rms_angular_velocity_dps = 460U;
    summary.v2.peak_angular_velocity_dps = 770U;
    summary.v2.max_orientation_change_cdeg = 0U;
    summary.v2.total_angular_motion_cdeg = 31500U;
    summary.v2.battery_mv = 3320U;
    summary.v2.staging_untested = true;

    /* These legacy fields must not appear in the compact V2.2 payload. */
    summary.v2.battery_percent = 87U;
    summary.v2.contraction_candidate_count = 44U;
    summary.v2.rotation_candidate_count = 55U;
    summary.v2.combined_event_flags = 0xA5U;

    summary.bma_step_count = 0U;
    summary.bma_accel_x_mg = 207;
    summary.bma_accel_y_mg = -198;
    summary.bma_accel_z_mg = 956;

    return summary;
}

int main(void)
{
    static const uint8_t expected_v2_2[BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE] = {
        0x41U, 0x10U, 0x00U, 0x0BU, 0x8FU, 0xF8U, 0x0CU, 0x8DU,
        0x09U, 0x8CU, 0x09U, 0x94U, 0x09U, 0xFAU, 0xFFU, 0x01U,
        0x03U, 0x08U, 0x03U, 0x02U, 0x00U, 0x03U, 0x3BU, 0x5FU,
        0x2EU, 0x4DU, 0x00U, 0x3FU, 0x00U, 0x00U, 0x00U, 0x00U,
        0xCFU, 0x00U, 0x3AU, 0xFFU, 0xBCU, 0x03U
    };
    bolus_telemetry_summary_v2_2_t summary = BuildSummary();
    uint8_t payload[BOLUS_TELEMETRY_V4_MAX_PACKET_SIZE] = {0};
    size_t payload_size = 0U;

    memset(payload, 0xA5, sizeof(payload));
    assert(TelemetryCodec_EncodeSummaryV2_2(
               &summary, payload, BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE,
               &payload_size) == TELEMETRY_CODEC_OK);
    assert(payload_size == BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE);
    assert(memcmp(payload, expected_v2_2, sizeof(expected_v2_2)) == 0);
    assert(payload[BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE] == 0xA5U);

    payload_size = 99U;
    assert(TelemetryCodec_EncodeSummaryV2_2(
               &summary, payload, BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE - 1U,
               &payload_size) == TELEMETRY_CODEC_ERROR_BUFFER);
    assert(payload_size == 0U);

    /* Frozen V2.1 encoding remains available and preserves its legacy fields. */
    assert(TelemetryCodec_EncodeSummaryV2_1(
               &summary, payload, sizeof(payload), &payload_size) ==
           TELEMETRY_CODEC_OK);
    assert(payload_size == BOLUS_TELEMETRY_SUMMARY_V2_1_SIZE);
    assert(payload[0] == 0x31U);
    assert(payload[5] == 87U);
    assert(payload[6] == 0xF8U && payload[7] == 0x0CU);
    assert(payload[29] == 44U && payload[30] == 55U && payload[31] == 0xA5U);


    {
        bolus_event_digest_t digests[3] = {0};
        uint8_t consumed = 0U;
        bool more = false;

        digests[0].start_offset_s = 10U;
        digests[0].duration_s = 65U;
        digests[0].pulse_count = 3U;
        digests[0].mean_inter_pulse_interval_s = 12U;
        digests[0].suppressed_trigger_count = 2U;
        digests[0].temperature_min_centi_c = 3500;
        digests[0].temperature_max_centi_c = 3900;
        digests[0].temperature_end_centi_c = 3700;
        digests[0].rms_dynamic_accel_mg = 400U;
        digests[0].peak_dynamic_accel_mg = 1000U;
        digests[0].peak_angular_velocity_dps = 250U;
        digests[0].orientation_change_cdeg = 600U;
        digests[0].flags =
            BOLUS_EVENT_DIGEST_FLAG_MOTION |
            BOLUS_EVENT_DIGEST_FLAG_TEMP_LOW |
            BOLUS_EVENT_DIGEST_FLAG_TEMP_VALID |
            BOLUS_EVENT_DIGEST_FLAG_MPU_USED;
        digests[0].close_reason = BOLUS_EVENT_CLOSE_TELEMETRY_BOUNDARY;
        digests[0].mpu_burst_count = 2U;

        digests[1] = digests[0];
        digests[1].start_offset_s = 100U;
        digests[1].flags = BOLUS_EVENT_DIGEST_FLAG_TEMP_VALID;
        digests[1].pulse_count = 0U;
        digests[1].close_reason = BOLUS_EVENT_CLOSE_TEMP_NORMAL;

        digests[2] = digests[0];
        digests[2].start_offset_s = 220U;
        digests[2].flags |= BOLUS_EVENT_DIGEST_FLAG_CARRY;

        assert(TelemetryCodec_V4PacketCount(1U) == 1U);
        assert(TelemetryCodec_V4PacketCount(2U) == 1U);
        assert(TelemetryCodec_V4PacketCount(3U) == 2U);
        assert(TelemetryCodec_V4PacketCount(8U) == 4U);

        memset(payload, 0, sizeof(payload));
        assert(TelemetryCodec_EncodeSummaryV4(
                   &summary,
                   3U,
                   false,
                   86U,
                   300U,
                   6U,
                   1U,
                   8U,
                   digests,
                   3U,
                   payload,
                   sizeof(payload),
                   &payload_size,
                   &consumed,
                   &more) == TELEMETRY_CODEC_OK);

        assert(payload_size == BOLUS_TELEMETRY_V4_MAX_PACKET_SIZE);
        assert(consumed == 2U);
        assert(more);
        assert(payload[0] == 0x61U);
        assert(payload[1] == 0x10U && payload[2] == 0x00U);
        assert(payload[4] == 0x83U);
        assert(payload[8] == 86U && payload[9] == 0U);
        assert(payload[10] == 3U && payload[11] == 0U);
        assert(payload[12] == 0x2CU && payload[13] == 0x01U);
        assert(payload[14] == 6U);
        assert(payload[15] == 1U);
        assert(payload[16] == 3U);
        assert(payload[17] == 2U);
        assert(payload[18] == 8U);

        assert(payload[19] == 10U && payload[20] == 0U);
        assert(payload[21] == 13U);
        assert(payload[22] == 3U);
        assert(payload[23] == 12U);
        assert(payload[24] == 2U);
        assert(payload[25] == 140U);
        assert(payload[26] == 156U);
        assert(payload[27] == 148U);
        assert(payload[28] == 20U);
        assert(payload[29] == 50U);
        assert(payload[30] == 25U);
        assert(payload[31] == 3U);
        assert(payload[32] == 0x2DU);
        assert(payload[33] == BOLUS_EVENT_CLOSE_TELEMETRY_BOUNDARY);
        assert(payload[34] == 2U);

        assert(TelemetryCodec_EncodeContinuationV4(
                   summary.v2.sequence,
                   1U,
                   2U,
                   3U,
                   &digests[2],
                   1U,
                   payload,
                   sizeof(payload),
                   &payload_size,
                   &consumed,
                   &more) == TELEMETRY_CODEC_OK);
        assert(payload_size == 23U);
        assert(consumed == 1U);
        assert(!more);
        assert(payload[0] == 0x62U);
        assert(payload[3] == 1U);
        assert(payload[4] == 2U);
        assert(payload[5] == 3U);
        assert(payload[6] == 1U);
        assert(payload[20] & BOLUS_EVENT_DIGEST_FLAG_CARRY);
    }

    return 0;
}
