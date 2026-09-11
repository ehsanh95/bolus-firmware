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
    uint8_t payload[BOLUS_TELEMETRY_SUMMARY_V2_1_SIZE] = {0};
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

    return 0;
}
