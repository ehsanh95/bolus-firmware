#include "sensor_service.h"

#include "bma456_motion.h"
#include "tmp117.h"
#include "mpu6050_motion.h"
#include "bolus_power.h"
#include "fault_manager.h"

#include <limits.h>
#include <string.h>

#define SENSOR_SERVICE_BMA_POWER_STABILIZE_MS    10U
#define SENSOR_SERVICE_TMP_POWER_STABILIZE_MS    10U
#define SENSOR_SERVICE_TMP_POLL_MS                2U
#define SENSOR_SERVICE_TMP_DEVICE_ID_EXPECTED     0x0117U
#define SENSOR_SERVICE_TMP_DRDY_MASK              (1U << 13)
#define SENSOR_SERVICE_MPU_POWER_STABILIZE_MS    10U
#define SENSOR_SERVICE_GRAVITY_MG                 1000UL

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

static bool s_mpu_ready = false;
static I2C_HandleTypeDef *s_mpu_i2c = NULL;
static mpu6050_motion_config_t s_mpu_config = {0};
static uint16_t s_mpu_burst_duration_ms = 250U;
static bool s_mpu_event_trigger_enable = false;

static uint32_t IntegerSqrtU64(uint64_t value)
{
    uint64_t result = 0U;
    uint64_t bit = (uint64_t)1U << 62;

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

    return (result > UINT32_MAX) ? UINT32_MAX : (uint32_t)result;
}

static uint16_t SaturateU16(uint32_t value)
{
    return (value > UINT16_MAX) ? UINT16_MAX : (uint16_t)value;
}

static int16_t SaturateS16(int32_t value)
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

static int32_t WrapAngleDeltaMdeg(int32_t delta_mdeg)
{
    while (delta_mdeg > 180000L)
    {
        delta_mdeg -= 360000L;
    }

    while (delta_mdeg < -180000L)
    {
        delta_mdeg += 360000L;
    }

    return delta_mdeg;
}

static uint32_t VectorMagnitude3(int32_t x, int32_t y, int32_t z)
{
    uint64_t sum_sq =
        ((uint64_t)((int64_t)x * (int64_t)x)) +
        ((uint64_t)((int64_t)y * (int64_t)y)) +
        ((uint64_t)((int64_t)z * (int64_t)z));

    return IntegerSqrtU64(sum_sq);
}

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
            sample->step_delta =
                current_step_total - s_bma_last_step_total;
        }
        else
        {
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

    ok = TMP117_setConversionMode(
        s_tmp_i2c,
        s_tmp_buffer,
        TMP117_OS_MODE);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_READ;
    }

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

    (void)TMP117_setConversionMode(
        s_tmp_i2c,
        s_tmp_buffer,
        TMP117_SD_MODE);

    if (!ok)
    {
        FaultManager_Raise(BOLUS_FAULT_TMP117_COMM);
        return SENSOR_SERVICE_ERROR_TMP_READ;
    }

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

sensor_service_status_t SensorService_InitMpu(
    I2C_HandleTypeDef *hi2c,
    const bolus_runtime_config_t *config)
{
    mpu6050_motion_status_t status;

    if ((hi2c == NULL) || (config == NULL))
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    s_mpu_ready = false;
    s_mpu_i2c = hi2c;
    s_mpu_config.sample_rate_hz = config->mpu.sample_rate_hz;
    s_mpu_config.accel_range_g = config->mpu.accel_range_g;
    s_mpu_config.gyro_range_dps = config->mpu.gyro_range_dps;
    s_mpu_burst_duration_ms = config->mpu.burst_duration_ms;
    s_mpu_event_trigger_enable = config->mpu.event_trigger_enable;

    BolusPower_On(BOLUS_POWER_MPU6050);
    HAL_Delay(SENSOR_SERVICE_MPU_POWER_STABILIZE_MS);

    status = MPU6050Motion_Init(s_mpu_i2c, &s_mpu_config);

    if (status == MPU6050_MOTION_OK)
    {
        (void)MPU6050Motion_Sleep();
    }

    BolusPower_Off(BOLUS_POWER_MPU6050);

    if (status != MPU6050_MOTION_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_MPU6050_INIT);

        if (status == MPU6050_MOTION_ERROR_COMM)
        {
            FaultManager_Raise(BOLUS_FAULT_MPU6050_COMM);
        }

        return SENSOR_SERVICE_ERROR_MPU_INIT;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_INIT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID);

    s_mpu_ready = true;
    return SENSOR_SERVICE_OK;
}

sensor_service_status_t SensorService_ReadMpuSample(
    sensor_service_mpu_sample_t *sample)
{
    mpu6050_motion_status_t status;
    mpu6050_motion_sample_t motion_sample = {0};

    if (sample == NULL)
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if ((!s_mpu_ready) || (s_mpu_i2c == NULL))
    {
        FaultManager_Raise(BOLUS_FAULT_MPU6050_INIT);
        return SENSOR_SERVICE_ERROR_MPU_READ;
    }

    BolusPower_On(BOLUS_POWER_MPU6050);
    HAL_Delay(SENSOR_SERVICE_MPU_POWER_STABILIZE_MS);

    status = MPU6050Motion_Init(s_mpu_i2c, &s_mpu_config);

    if (status == MPU6050_MOTION_OK)
    {
        status = MPU6050Motion_ReadSample(&motion_sample);
    }

    if (MPU6050Motion_IsReady())
    {
        (void)MPU6050Motion_Sleep();
    }

    BolusPower_Off(BOLUS_POWER_MPU6050);

    if (status != MPU6050_MOTION_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_MPU6050_COMM);
        return SENSOR_SERVICE_ERROR_MPU_READ;
    }

    sample->accel_x_mg = motion_sample.accel_x_mg;
    sample->accel_y_mg = motion_sample.accel_y_mg;
    sample->accel_z_mg = motion_sample.accel_z_mg;

    sample->gyro_x_mdps = motion_sample.gyro_x_mdps;
    sample->gyro_y_mdps = motion_sample.gyro_y_mdps;
    sample->gyro_z_mdps = motion_sample.gyro_z_mdps;

    sample->temperature_mdeg_c = motion_sample.temperature_mdeg_c;

    sample->sample_rate_hz = s_mpu_config.sample_rate_hz;
    sample->accel_range_g = s_mpu_config.accel_range_g;
    sample->gyro_range_dps = s_mpu_config.gyro_range_dps;

    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_INIT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_TIMEOUT);

    return SENSOR_SERVICE_OK;
}

sensor_service_status_t SensorService_ReadMpuBurst(
    sensor_service_mpu_burst_features_t *features)
{
    mpu6050_motion_status_t status = MPU6050_MOTION_ERROR_NOT_READY;
    mpu6050_motion_status_t end_status;
    mpu6050_motion_sample_t sample = {0};
    uint32_t target_samples;
    uint32_t sample_interval_ms;
    uint32_t next_sample_tick;
    uint32_t previous_sample_tick = 0U;
    uint64_t dynamic_sum_sq_mg2 = 0U;
    uint64_t gyro_sum_sq_mdps2 = 0U;
    uint64_t total_angular_motion_cdeg = 0U;
    uint32_t peak_dynamic_mg = 0U;
    uint32_t peak_gyro_mdps = 0U;
    uint16_t sample_count = 0U;
    bool first_orientation_valid = false;
    bool last_orientation_valid = false;
    int32_t first_roll_mdeg = 0;
    int32_t first_pitch_mdeg = 0;
    int32_t last_roll_mdeg = 0;
    int32_t last_pitch_mdeg = 0;
    bool burst_started = false;

    if (features == NULL)
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    memset(features, 0, sizeof(*features));

    if ((!s_mpu_ready) || (s_mpu_i2c == NULL))
    {
        FaultManager_Raise(BOLUS_FAULT_MPU6050_INIT);
        return SENSOR_SERVICE_ERROR_MPU_READ;
    }

    if (!s_mpu_event_trigger_enable)
    {
        return SENSOR_SERVICE_ERROR_CONFIG;
    }

    features->configured_duration_ms = s_mpu_burst_duration_ms;
    features->sample_rate_hz = s_mpu_config.sample_rate_hz;
    features->accel_range_g = s_mpu_config.accel_range_g;
    features->gyro_range_dps = s_mpu_config.gyro_range_dps;

    target_samples =
        ((uint32_t)s_mpu_burst_duration_ms * (uint32_t)s_mpu_config.sample_rate_hz) /
        1000UL;

    if (target_samples == 0U)
    {
        target_samples = 1U;
    }

    if (target_samples > UINT16_MAX)
    {
        target_samples = UINT16_MAX;
    }

    sample_interval_ms = 1000UL / (uint32_t)s_mpu_config.sample_rate_hz;
    if (sample_interval_ms == 0U)
    {
        sample_interval_ms = 1U;
    }

    /* One power-up/configure cycle for the whole burst. */
    BolusPower_On(BOLUS_POWER_MPU6050);
    HAL_Delay(SENSOR_SERVICE_MPU_POWER_STABILIZE_MS);

    status = MPU6050Motion_Init(s_mpu_i2c, &s_mpu_config);
    if (status == MPU6050_MOTION_OK)
    {
        status = MPU6050Motion_BeginBurst();
        burst_started = (status == MPU6050_MOTION_OK);
    }

    next_sample_tick = HAL_GetTick();

    while ((status == MPU6050_MOTION_OK) &&
           ((uint32_t)sample_count < target_samples))
    {
        uint32_t sample_tick;
        uint32_t accel_mag_mg;
        uint32_t dynamic_mg;
        uint32_t gyro_mag_mdps;

        while ((int32_t)(HAL_GetTick() - next_sample_tick) < 0)
        {
            HAL_Delay(1U);
        }

        status = MPU6050Motion_ReadBurstSample(&sample);
        if (status != MPU6050_MOTION_OK)
        {
            break;
        }

        sample_tick = HAL_GetTick();

        accel_mag_mg = VectorMagnitude3(
            sample.accel_x_mg,
            sample.accel_y_mg,
            sample.accel_z_mg);
        dynamic_mg =
            (accel_mag_mg >= SENSOR_SERVICE_GRAVITY_MG)
                ? (accel_mag_mg - SENSOR_SERVICE_GRAVITY_MG)
                : (SENSOR_SERVICE_GRAVITY_MG - accel_mag_mg);

        gyro_mag_mdps = VectorMagnitude3(
            sample.gyro_x_mdps,
            sample.gyro_y_mdps,
            sample.gyro_z_mdps);

        if (dynamic_mg > peak_dynamic_mg)
        {
            peak_dynamic_mg = dynamic_mg;
        }

        if (gyro_mag_mdps > peak_gyro_mdps)
        {
            peak_gyro_mdps = gyro_mag_mdps;
        }

        dynamic_sum_sq_mg2 +=
            (uint64_t)dynamic_mg * (uint64_t)dynamic_mg;
        gyro_sum_sq_mdps2 +=
            (uint64_t)gyro_mag_mdps * (uint64_t)gyro_mag_mdps;

        if (sample_count > 0U)
        {
            uint32_t dt_ms = sample_tick - previous_sample_tick;
            total_angular_motion_cdeg +=
                ((uint64_t)gyro_mag_mdps * (uint64_t)dt_ms) / 10000ULL;
        }

        if (mpu6050_diag_orientation_valid)
        {
            if (!first_orientation_valid)
            {
                first_orientation_valid = true;
                first_roll_mdeg = mpu6050_diag_roll_mdeg;
                first_pitch_mdeg = mpu6050_diag_pitch_mdeg;
            }

            last_orientation_valid = true;
            last_roll_mdeg = mpu6050_diag_roll_mdeg;
            last_pitch_mdeg = mpu6050_diag_pitch_mdeg;
        }

        previous_sample_tick = sample_tick;
        sample_count++;
        next_sample_tick += sample_interval_ms;
    }

    if (burst_started || MPU6050Motion_IsBurstActive())
    {
        end_status = MPU6050Motion_EndBurst();
        if ((status == MPU6050_MOTION_OK) &&
            (end_status != MPU6050_MOTION_OK))
        {
            status = end_status;
        }
    }
    else if (MPU6050Motion_IsReady())
    {
        (void)MPU6050Motion_Sleep();
    }

    /* Physical rail OFF is mandatory even on read/config failure. */
    BolusPower_Off(BOLUS_POWER_MPU6050);

    if ((status != MPU6050_MOTION_OK) || (sample_count == 0U))
    {
        FaultManager_Raise(BOLUS_FAULT_MPU6050_COMM);
        return SENSOR_SERVICE_ERROR_MPU_READ;
    }

    features->sample_count = sample_count;
    features->peak_dynamic_accel_mg = SaturateU16(peak_dynamic_mg);
    features->rms_dynamic_accel_mg = SaturateU16(
        IntegerSqrtU64(dynamic_sum_sq_mg2 / (uint64_t)sample_count));

    features->peak_angular_velocity_dps = SaturateU16(
        (peak_gyro_mdps + 500UL) / 1000UL);
    features->rms_angular_velocity_dps = SaturateU16(
        (IntegerSqrtU64(gyro_sum_sq_mdps2 / (uint64_t)sample_count) + 500UL) /
        1000UL);

    features->total_angular_motion_cdeg =
        (total_angular_motion_cdeg > UINT32_MAX)
            ? UINT32_MAX
            : (uint32_t)total_angular_motion_cdeg;

    if (first_orientation_valid && last_orientation_valid)
    {
        int32_t roll_delta_mdeg =
            WrapAngleDeltaMdeg(last_roll_mdeg - first_roll_mdeg);
        int32_t pitch_delta_mdeg =
            WrapAngleDeltaMdeg(last_pitch_mdeg - first_pitch_mdeg);
        uint32_t orientation_change_mdeg = VectorMagnitude3(
            roll_delta_mdeg,
            pitch_delta_mdeg,
            0);

        features->orientation_change_valid = true;
        features->roll_change_cdeg = SaturateS16(roll_delta_mdeg / 10L);
        features->pitch_change_cdeg = SaturateS16(pitch_delta_mdeg / 10L);
        features->orientation_change_cdeg =
            SaturateU16((orientation_change_mdeg + 5UL) / 10UL);
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_INIT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_MPU6050_TIMEOUT);

    return SENSOR_SERVICE_OK;
}

bool SensorService_IsMpuReady(void)
{
    return s_mpu_ready;
}
