#include "sensor_service.h"

#include "bma456_motion.h"
#include "bolus_power.h"
#include "fault_manager.h"

#define SENSOR_SERVICE_BMA_POWER_STABILIZE_MS  10U

static bool s_bma_ready = false;
static bool s_bma_step_enabled = false;
static bool s_bma_step_baseline_valid = false;
static uint32_t s_bma_last_step_total = 0U;
static bolus_bma_step_sensitivity_t s_bma_step_sensitivity =
    BOLUS_BMA_STEP_SENSITIVITY_DEFAULT;

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
            /*
             * Unsigned subtraction intentionally handles uint32 wraparound.
             */
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
