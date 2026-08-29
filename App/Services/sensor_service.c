#include "sensor_service.h"

#include "bma456_motion.h"
#include "tmp117.h"
#include "bolus_power.h"
#include "fault_manager.h"

#define SENSOR_SERVICE_BMA_POWER_STABILIZE_MS    10U
#define SENSOR_SERVICE_TMP_POWER_STABILIZE_MS    10U
#define SENSOR_SERVICE_TMP_POLL_MS                2U
#define SENSOR_SERVICE_TMP_DEVICE_ID_EXPECTED     0x0117U
#define SENSOR_SERVICE_TMP_DRDY_MASK              (1U << 13)

static bool s_bma_ready = false;
static bool s_bma_step_enabled = false;
static bool s_bma_step_baseline_valid = false;
static uint32_t s_bma_last_step_total = 0U;
static bolus_bma_step_sensitivity_t s_bma_step_sensitivity =
    BOLUS_BMA_STEP_SENSITIVITY_DEFAULT;

static bool s_tmp_ready = false;
static I2C_HandleTypeDef *s_tmp_i2c = NULL;
static uint8_t s_tmp_buffer[3] = {0};
static uint16_t s_tmp_device_id = 0U;
static uint8_t s_tmp_averaging_samples = 1U;
static uint32_t s_tmp_conversion_timeout_ms = 50U;

static bool MapBmaOdr(
    bolus_bma_odr_t odr,
    bma456_motion_odr_t *mapped)
{
    if (mapped == NULL)
    {
        return false;
    }

    switch (odr)
    {
        case BOLUS_BMA_ODR_6_25_HZ:
            *mapped = BMA456_MOTION_ODR_6_25_HZ;
            return true;

        case BOLUS_BMA_ODR_12_5_HZ:
            *mapped = BMA456_MOTION_ODR_12_5_HZ;
            return true;

        case BOLUS_BMA_ODR_25_HZ:
            *mapped = BMA456_MOTION_ODR_25_HZ;
            return true;

        case BOLUS_BMA_ODR_50_HZ:
            *mapped = BMA456_MOTION_ODR_50_HZ;
            return true;

        default:
            return false;
    }
}

static bool MapTmpAveraging(
    uint8_t averaging_samples,
    TMP117_AVG_MODE *mode,
    uint32_t *timeout_ms)
{
    if ((mode == NULL) || (timeout_ms == NULL))
    {
        return false;
    }

    switch (averaging_samples)
    {
        case 1U:
            *mode = TMP117_NO_AVG;
            *timeout_ms = 50U;
            return true;

        case 8U:
            *mode = TMP117_AVG_8;
            *timeout_ms = 175U;
            return true;

        case 32U:
            *mode = TMP117_AVG_32;
            *timeout_ms = 550U;
            return true;

        case 64U:
            *mode = TMP117_AVG_64;
            *timeout_ms = 1050U;
            return true;

        default:
            return false;
    }
}

static int32_t TemperatureCToMilliC(double temperature_c)
{
    double scaled = temperature_c * 1000.0;

    if (scaled >= 0.0)
    {
        return (int32_t)(scaled + 0.5);
    }

    return (int32_t)(scaled - 0.5);
}

sensor_service_status_t SensorService_InitBma(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config)
{
    bma456_motion_config_t motion_config;
    bma456_motion_status_t status;

    if ((hspi == NULL) || (config == NULL))
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    if (!MapBmaOdr(config->bma.odr, &motion_config.odr))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    motion_config.range_g = config->bma.range_g;
    motion_config.averaging_samples = config->bma.averaging_samples;
    motion_config.step_counter_enable = config->bma.step_counter_enable;
    motion_config.step_sensitivity_level =
        (uint8_t)config->bma.step_sensitivity;

    s_bma_ready = false;
    s_bma_step_enabled = false;
    s_bma_step_baseline_valid = false;
    s_bma_last_step_total = 0U;
    s_bma_step_sensitivity = config->bma.step_sensitivity;

    /*
     * Temporary Phase 5 power ownership:
     * SensorService controls the BMA rail directly until PowerService is
     * introduced. In normal product operation the BMA rail stays on while
     * the MCU sleeps.
     */
    BolusPower_On(BOLUS_POWER_BMA456);
    HAL_Delay(SENSOR_SERVICE_BMA_POWER_STABILIZE_MS);

    status = BMA456Motion_Init(hspi, &motion_config);

    if (status != BMA456_MOTION_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return SENSOR_SERVICE_ERROR_BMA_INIT;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID);

    s_bma_step_enabled = BMA456Motion_IsStepCounterEnabled();
    s_bma_ready = true;

    return SENSOR_SERVICE_OK;
}

sensor_service_status_t SensorService_ReadBmaSample(
    sensor_service_bma_sample_t *sample)
{
    bma456_motion_status_t status;
    uint32_t current_step_total = 0U;

    if (sample == NULL)
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if (!s_bma_ready)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return SENSOR_SERVICE_ERROR_BMA_READ;
    }

    sample->step_total = 0U;
    sample->step_delta = 0U;
    sample->step_sensitivity = s_bma_step_sensitivity;

    status = BMA456Motion_ReadAccelMg(
        &sample->x_mg,
        &sample->y_mg,
        &sample->z_mg);

    if (status != BMA456_MOTION_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return SENSOR_SERVICE_ERROR_BMA_READ;
    }

    if (s_bma_step_enabled)
    {
        status = BMA456Motion_ReadStepCount(&current_step_total);

        if (status != BMA456_MOTION_OK)
        {
            FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
            return SENSOR_SERVICE_ERROR_BMA_READ;
        }

        sample->step_total = current_step_total;

        if (s_bma_step_baseline_valid)
        {
            /* Unsigned subtraction intentionally handles uint32 wraparound. */
            sample->step_delta =
                current_step_total - s_bma_last_step_total;
        }
        else
        {
            /* First successful read establishes the interval baseline. */
            sample->step_delta = 0U;
            s_bma_step_baseline_valid = true;
        }

        s_bma_last_step_total = current_step_total;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);

    return SENSOR_SERVICE_OK;
}

bool SensorService_IsBmaReady(void)
{
    return s_bma_ready && BMA456Motion_IsReady();
}

sensor_service_status_t SensorService_InitTemperature(
    I2C_HandleTypeDef *hi2c,
    const bolus_runtime_config_t *config)
{
    TMP117_AVG_MODE averaging_mode;
    uint32_t conversion_timeout_ms;
    bool ok;

    if ((hi2c == NULL) || (config == NULL))
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    if (!MapTmpAveraging(
            config->temperature.averaging_samples,
            &averaging_mode,
            &conversion_timeout_ms))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    s_tmp_ready = false;
    s_tmp_i2c = hi2c;
    s_tmp_device_id = 0U;
    s_tmp_averaging_samples = config->temperature.averaging_samples;
    s_tmp_conversion_timeout_ms = conversion_timeout_ms;

    /*
     * Temporary Phase-5 power policy: keep the rail powered and put TMP117 in
     * shutdown between one-shot conversions. PowerService later decides
     * whether rail-off gives a worthwhile system-level leakage improvement.
     */
    BolusPower_On(BOLUS_POWER_TMP117);
    HAL_Delay(SENSOR_SERVICE_TMP_POWER_STABILIZE_MS);

    ok = TMP117_Init(s_tmp_i2c, s_tmp_buffer);
    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_INIT;
    }

    ok = TMP117_getDeviceID(
        s_tmp_i2c,
        s_tmp_buffer,
        &s_tmp_device_id);

    if ((!ok) || (s_tmp_device_id != SENSOR_SERVICE_TMP_DEVICE_ID_EXPECTED))
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_INIT;
    }

    ok = TMP117_setAveraging(
        s_tmp_i2c,
        s_tmp_buffer,
        averaging_mode);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_INIT;
    }

    if (config->temperature.alert_enable)
    {
        ok = TMP117_setHighLimitTemperature(
            s_tmp_i2c,
            s_tmp_buffer,
            ((double)config->temperature.high_limit_centi_c) / 100.0);

        if (ok)
        {
            ok = TMP117_setLowLimitTemperature(
                s_tmp_i2c,
                s_tmp_buffer,
                ((double)config->temperature.low_limit_centi_c) / 100.0);
        }

        if (ok)
        {
            ok = TMP117_setAlertMode(
                s_tmp_i2c,
                s_tmp_buffer,
                TMP117_ALERT_MODE);
        }

        if (!ok)
        {
            FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
            return SENSOR_SERVICE_ERROR_TMP_INIT;
        }
    }

    ok = TMP117_setConversionMode(
        s_tmp_i2c,
        s_tmp_buffer,
        TMP117_SD_MODE);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_INIT;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_TIMEOUT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_INVALID_DATA);
    (void)FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID);

    s_tmp_ready = true;

    return SENSOR_SERVICE_OK;
}

sensor_service_status_t SensorService_ReadTemperatureOneShot(
    sensor_service_temperature_sample_t *sample)
{
    uint32_t start_tick;
    uint16_t config_reg = 0U;
    double temperature_c = 0.0;
    bool ok;

    if (sample == NULL)
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    sample->temperature_mdeg_c = 0;
    sample->device_id = s_tmp_device_id;
    sample->averaging_samples = s_tmp_averaging_samples;

    if ((!s_tmp_ready) || (s_tmp_i2c == NULL))
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_READ;
    }

    /* Trigger a single conversion from shutdown. */
    ok = TMP117_setConversionMode(
        s_tmp_i2c,
        s_tmp_buffer,
        TMP117_OS_MODE);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_READ;
    }

    /*
     * Bounded wait for Data Ready. Normal AVG1 completes in about 15.5 ms;
     * larger averaging modes are allowed by RuntimeConfig and have explicit
     * finite timeouts here so a failed sensor cannot hold the system hostage.
     */
    start_tick = HAL_GetTick();

    while (true)
    {
        ok = TMP117_getConfig(
            s_tmp_i2c,
            s_tmp_buffer,
            &config_reg);

        if (!ok)
        {
            (void)TMP117_setConversionMode(
                s_tmp_i2c,
                s_tmp_buffer,
                TMP117_SD_MODE);

            FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
            return SENSOR_SERVICE_ERROR_TMP_READ;
        }

        if ((config_reg & SENSOR_SERVICE_TMP_DRDY_MASK) != 0U)
        {
            break;
        }

        if ((HAL_GetTick() - start_tick) >= s_tmp_conversion_timeout_ms)
        {
            (void)TMP117_setConversionMode(
                s_tmp_i2c,
                s_tmp_buffer,
                TMP117_SD_MODE);

            FaultManager_Raise(BOLUS_FAULT_TMP117_TIMEOUT);
            return SENSOR_SERVICE_ERROR_TMP_TIMEOUT;
        }

        HAL_Delay(SENSOR_SERVICE_TMP_POLL_MS);
    }

    ok = TMP117_getResultTemperature(
        s_tmp_i2c,
        s_tmp_buffer,
        &temperature_c);

    /* Explicitly return to shutdown even though one-shot is self-terminating. */
    (void)TMP117_setConversionMode(
        s_tmp_i2c,
        s_tmp_buffer,
        TMP117_SD_MODE);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_READ;
    }

    /* TMP117 specified operating range: reject obvious corrupted values. */
    if ((temperature_c < -55.0) || (temperature_c > 150.0))
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_INVALID_DATA);
        return SENSOR_SERVICE_ERROR_TMP_INVALID_DATA;
    }

    sample->temperature_mdeg_c = TemperatureCToMilliC(temperature_c);
    sample->device_id = s_tmp_device_id;
    sample->averaging_samples = s_tmp_averaging_samples;

    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_TIMEOUT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_TMP117_INVALID_DATA);

    return SENSOR_SERVICE_OK;
}

bool SensorService_IsTemperatureReady(void)
{
    return s_tmp_ready;
}
