#include "bolus_runtime_config.h"

#include <string.h>

static bool IsPowerOfTwo(uint8_t value)
{
    return ((value != 0U) && ((value & (uint8_t)(value - 1U)) == 0U));
}

void BolusRuntimeConfig_LoadDefaults(bolus_runtime_config_t *config)
{
    if (config == NULL)
    {
        return;
    }

    memset(config, 0, sizeof(*config));

    config->version = BOLUS_RUNTIME_CONFIG_VERSION;
    config->operating_mode = BOLUS_MODE_NORMAL;

    /* TMP117: periodic one-shot by default. */
    config->temperature.sample_period_s = 600U;
    config->temperature.strategy = BOLUS_TEMP_STRATEGY_PERIODIC;
    config->temperature.averaging_samples = 1U;
    config->temperature.alert_enable = false;
    config->temperature.high_limit_centi_c = 4100;
    config->temperature.low_limit_centi_c = 3500;

    /* BMA456: low-power continuous rumen-motion sentinel. */
    config->bma.odr = BOLUS_BMA_ODR_12_5_HZ;
    config->bma.range_g = 4U;
    config->bma.averaging_samples = 4U;
    config->bma.step_counter_enable = true;

    /*
     * Phase-5 bench characterization: exercise the middle Bolus sensitivity
     * profile while keeping the proven 12.5 Hz / continuous-mode step path.
     * Field trials will later select the production profile through downlink.
     */
    config->bma.step_sensitivity = BOLUS_BMA_STEP_SENSITIVITY_LEVEL_4;

    config->bma.fifo_enable = true;
    config->bma.motion_interrupt_enable = false;

    /* MPU6050: short detailed burst; event triggering is enabled later. */
    config->mpu.scheduled_period_s = 900U;
    config->mpu.burst_duration_ms = 1000U;
    config->mpu.sample_rate_hz = 100U;
    config->mpu.accel_range_g = 4U;
    config->mpu.gyro_range_dps = 500U;
    config->mpu.event_trigger_enable = false;

    /* Radio defaults remain conservative development values. */
    config->radio.uplink_period_s = 900U;
    config->radio.tx_power_dbm = 10;
    config->radio.spreading_factor = 7U;
    config->radio.bandwidth_index = 0U;
    config->radio.coding_rate = 1U;
}

bool BolusRuntimeConfig_Validate(const bolus_runtime_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if (config->version != BOLUS_RUNTIME_CONFIG_VERSION)
    {
        return false;
    }

    if (config->operating_mode > BOLUS_MODE_CUSTOM)
    {
        return false;
    }

    if ((config->temperature.sample_period_s == 0U) ||
        (config->temperature.sample_period_s > 86400U))
    {
        return false;
    }

    if ((config->temperature.averaging_samples != 1U) &&
        (config->temperature.averaging_samples != 8U) &&
        (config->temperature.averaging_samples != 32U) &&
        (config->temperature.averaging_samples != 64U))
    {
        return false;
    }

    if (config->temperature.high_limit_centi_c <=
        config->temperature.low_limit_centi_c)
    {
        return false;
    }

    if (config->bma.odr > BOLUS_BMA_ODR_50_HZ)
    {
        return false;
    }

    if ((config->bma.range_g != 2U) &&
        (config->bma.range_g != 4U) &&
        (config->bma.range_g != 8U) &&
        (config->bma.range_g != 16U))
    {
        return false;
    }

    if ((!IsPowerOfTwo(config->bma.averaging_samples)) ||
        (config->bma.averaging_samples > 64U))
    {
        return false;
    }

    if (config->bma.step_sensitivity > BOLUS_BMA_STEP_SENSITIVITY_LEVEL_7)
    {
        return false;
    }

    if ((config->mpu.burst_duration_ms < 100U) ||
        (config->mpu.burst_duration_ms > 10000U))
    {
        return false;
    }

    if ((config->mpu.sample_rate_hz < 10U) ||
        (config->mpu.sample_rate_hz > 1000U))
    {
        return false;
    }

    if ((config->mpu.accel_range_g != 2U) &&
        (config->mpu.accel_range_g != 4U) &&
        (config->mpu.accel_range_g != 8U) &&
        (config->mpu.accel_range_g != 16U))
    {
        return false;
    }

    if ((config->mpu.gyro_range_dps != 250U) &&
        (config->mpu.gyro_range_dps != 500U) &&
        (config->mpu.gyro_range_dps != 1000U) &&
        (config->mpu.gyro_range_dps != 2000U))
    {
        return false;
    }

    if ((config->radio.uplink_period_s == 0U) ||
        (config->radio.uplink_period_s > 86400U))
    {
        return false;
    }

    if ((config->radio.tx_power_dbm < 2) ||
        (config->radio.tx_power_dbm > 20))
    {
        return false;
    }

    if ((config->radio.spreading_factor < 7U) ||
        (config->radio.spreading_factor > 12U))
    {
        return false;
    }

    if (config->radio.bandwidth_index > 2U)
    {
        return false;
    }

    if ((config->radio.coding_rate < 1U) ||
        (config->radio.coding_rate > 4U))
    {
        return false;
    }

    return true;
}
