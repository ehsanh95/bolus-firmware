#include "telemetry_codec.h"

#include <limits.h>
#include <string.h>

static uint8_t SaturateU8(uint32_t value)
{
    return (value > UINT8_MAX) ? UINT8_MAX : (uint8_t)value;
}

static int16_t SaturateI16(int32_t value)
{
    if (value > INT16_MAX)
    {
        return INT16_MAX;
    }

    if (value < INT16_MIN)
    {
        return INT16_MIN;
    }

    return (int16_t)value;
}

static uint32_t IntegerSqrtU32(uint32_t value)
{
    uint32_t result = 0U;
    uint32_t bit = 1UL << 30;

    while (bit > value)
    {
        bit >>= 2;
    }

    while (bit != 0U)
    {
        if (value >= (result + bit))
        {
            value -= result + bit;
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }

        bit >>= 2;
    }

    return result;
}

static int16_t MdegCToCentiC(int32_t temperature_mdeg_c)
{
    int32_t rounded_centi_c;

    if (temperature_mdeg_c >= 0)
    {
        rounded_centi_c = (temperature_mdeg_c + 5) / 10;
    }
    else
    {
        rounded_centi_c = (temperature_mdeg_c - 5) / 10;
    }

    return SaturateI16(rounded_centi_c);
}

static uint8_t QuantizeAccel20Mg(uint16_t acceleration_mg)
{
    return SaturateU8(((uint32_t)acceleration_mg + 10U) / 20U);
}

static uint8_t QuantizeDps10(uint16_t angular_velocity_dps)
{
    return SaturateU8(((uint32_t)angular_velocity_dps + 5U) / 10U);
}

static uint8_t QuantizeOrientation2Deg(uint16_t orientation_change_cdeg)
{
    return SaturateU8(((uint32_t)orientation_change_cdeg + 100U) / 200U);
}

static uint8_t QuantizeAngularMotion5Deg(uint32_t angular_motion_cdeg)
{
    return SaturateU8((angular_motion_cdeg + 250UL) / 500UL);
}

static void WriteU16Le(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)(value & 0xFFU);
    destination[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static void WriteI16Le(uint8_t *destination, int16_t value)
{
    WriteU16Le(destination, (uint16_t)value);
}

telemetry_codec_status_t TelemetryCodec_EncodeSummaryV1(
    const bolus_telemetry_summary_v1_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size)
{
    uint8_t status = 0U;
    uint8_t config_version;
    uint8_t battery_percent;
    uint8_t interval_std_s = 0U;

    if ((summary == NULL) || (payload == NULL) || (payload_size == NULL))
    {
        return TELEMETRY_CODEC_ERROR_PARAM;
    }

    *payload_size = 0U;

    if (payload_capacity < BOLUS_TELEMETRY_SUMMARY_V1_SIZE)
    {
        return TELEMETRY_CODEC_ERROR_BUFFER;
    }

    memset(payload, 0, BOLUS_TELEMETRY_SUMMARY_V1_SIZE);

    if (summary->temperature_valid)
    {
        status |= BOLUS_TELEMETRY_STATUS_TEMP_VALID;
    }

    if (summary->motion_valid)
    {
        status |= BOLUS_TELEMETRY_STATUS_MOTION_VALID;
    }

    if (summary->inter_event_interval_valid)
    {
        status |= BOLUS_TELEMETRY_STATUS_INTERVAL_VALID;
        interval_std_s = SaturateU8(
            IntegerSqrtU32(summary->inter_event_interval_variance_s2));
    }

    if (summary->event_record_overflow)
    {
        status |= BOLUS_TELEMETRY_STATUS_EVENT_OVERFLOW;
    }

    if (summary->fault_present)
    {
        status |= BOLUS_TELEMETRY_STATUS_FAULT_PRESENT;
    }

    if (summary->health_degraded)
    {
        status |= BOLUS_TELEMETRY_STATUS_HEALTH_DEGRADED;
    }

    if (summary->health_critical)
    {
        status |= BOLUS_TELEMETRY_STATUS_HEALTH_CRITICAL;
    }

    config_version = SaturateU8(summary->config_version);
    battery_percent =
        (summary->battery_percent > 100U) ? 100U : summary->battery_percent;

    payload[0] = (uint8_t)(
        ((BOLUS_TELEMETRY_PROTOCOL_VERSION_V1 & 0x0FU) << 4) |
        (BOLUS_TELEMETRY_MESSAGE_TYPE_SUMMARY & 0x0FU));
    WriteU16Le(&payload[1], summary->sequence);
    payload[3] = config_version;
    payload[4] = status;
    payload[5] = battery_percent;
    WriteU16Le(&payload[6], summary->battery_mv);

    if (summary->temperature_valid)
    {
        WriteI16Le(&payload[8], MdegCToCentiC(summary->temperature_current_mdeg_c));
        WriteI16Le(&payload[10], MdegCToCentiC(summary->temperature_min_mdeg_c));
        WriteI16Le(&payload[12], MdegCToCentiC(summary->temperature_max_mdeg_c));
        WriteI16Le(&payload[14], MdegCToCentiC(summary->max_negative_excursion_mdeg_c));
    }

    if (summary->motion_valid)
    {
        payload[16] = SaturateU8(summary->motion_event_count);
        payload[17] = SaturateU8(summary->contraction_candidate_count);
        payload[18] = SaturateU8(summary->rotation_candidate_count);
        payload[21] = QuantizeAccel20Mg(summary->rms_dynamic_accel_mg);
        payload[22] = QuantizeAccel20Mg(summary->peak_dynamic_accel_mg);
    }

    if (summary->inter_event_interval_valid)
    {
        payload[19] = SaturateU8(summary->inter_event_interval_mean_s);
        payload[20] = interval_std_s;
    }

    payload[23] = (uint8_t)(summary->combined_event_flags & 0xFFU);

    *payload_size = BOLUS_TELEMETRY_SUMMARY_V1_SIZE;
    return TELEMETRY_CODEC_OK;
}

telemetry_codec_status_t TelemetryCodec_EncodeSummaryV2(
    const bolus_telemetry_summary_v2_t *summary,
    uint8_t *payload,
    size_t payload_capacity,
    size_t *payload_size)
{
    uint8_t status = 0U;
    uint8_t interval_std_s = 0U;

    if ((summary == NULL) || (payload == NULL) || (payload_size == NULL))
    {
        return TELEMETRY_CODEC_ERROR_PARAM;
    }

    *payload_size = 0U;

    if (payload_capacity < BOLUS_TELEMETRY_SUMMARY_V2_SIZE)
    {
        return TELEMETRY_CODEC_ERROR_BUFFER;
    }

    memset(payload, 0, BOLUS_TELEMETRY_SUMMARY_V2_SIZE);

    if (summary->temperature_valid)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_TEMP_VALID;
    }

    if (summary->motion_valid)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_MOTION_VALID;
    }

    if (summary->inter_pulse_interval_valid)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_INTERVAL_VALID;
        interval_std_s = SaturateU8(
            IntegerSqrtU32(summary->inter_pulse_interval_variance_s2));
    }

    if (summary->mpu_valid)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_MPU_VALID;
    }

    if (summary->fault_present)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_FAULT_PRESENT;
    }

    if (summary->health_degraded)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_HEALTH_DEGRADED;
    }

    if (summary->health_critical)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_HEALTH_CRITICAL;
    }

    if (summary->staging_untested)
    {
        status |= BOLUS_TELEMETRY_V2_STATUS_STAGING_UNTESTED;
    }

    payload[0] = (uint8_t)(
        ((BOLUS_TELEMETRY_PROTOCOL_VERSION_V2 & 0x0FU) << 4) |
        (BOLUS_TELEMETRY_MESSAGE_TYPE_SUMMARY & 0x0FU));
    WriteU16Le(&payload[1], summary->sequence);
    payload[3] = SaturateU8(summary->config_version);
    payload[4] = status;
    payload[5] = (summary->battery_percent > 100U) ? 100U : summary->battery_percent;
    WriteU16Le(&payload[6], summary->battery_mv);

    if (summary->temperature_valid)
    {
        WriteI16Le(&payload[8], MdegCToCentiC(summary->temperature_current_mdeg_c));
        WriteI16Le(&payload[10], MdegCToCentiC(summary->temperature_min_mdeg_c));
        WriteI16Le(&payload[12], MdegCToCentiC(summary->temperature_max_mdeg_c));
        WriteI16Le(&payload[14], MdegCToCentiC(summary->max_negative_excursion_mdeg_c));
    }

    if (summary->motion_valid)
    {
        payload[16] = SaturateU8(summary->episode_count);
        payload[17] = SaturateU8(summary->accepted_pulse_count);
        payload[18] = SaturateU8(summary->suppressed_pulse_count);
        payload[19] = SaturateU8(summary->max_pulses_per_episode);
    }

    if (summary->inter_pulse_interval_valid)
    {
        payload[20] = SaturateU8(summary->inter_pulse_interval_mean_s);
        payload[21] = interval_std_s;
    }

    if (summary->mpu_valid)
    {
        payload[22] = SaturateU8(summary->mpu_burst_count);
        payload[23] = QuantizeAccel20Mg(summary->rms_dynamic_accel_mg);
        payload[24] = QuantizeAccel20Mg(summary->peak_dynamic_accel_mg);
        payload[25] = QuantizeDps10(summary->rms_angular_velocity_dps);
        payload[26] = QuantizeDps10(summary->peak_angular_velocity_dps);
        payload[27] = QuantizeOrientation2Deg(summary->max_orientation_change_cdeg);
        payload[28] = QuantizeAngularMotion5Deg(summary->total_angular_motion_cdeg);
    }

    payload[29] = SaturateU8(summary->contraction_candidate_count);
    payload[30] = SaturateU8(summary->rotation_candidate_count);
    payload[31] = (uint8_t)(summary->combined_event_flags & 0xFFU);

    *payload_size = BOLUS_TELEMETRY_SUMMARY_V2_SIZE;
    return TELEMETRY_CODEC_OK;
}
