#include "sensor_service.h"

#include "bma456_motion.h"
#include "bolus_power.h"
#include "fault_manager.h"

#define SENSOR_SERVICE_BMA_POWER_STABILIZE_MS  10U

static bool s_bma_ready = false;

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

    s_bma_ready = false;

    /*
     * Temporary Phase 5 power ownership:
     * SensorService controls the BMA rail directly until PowerService is
     * introduced. The public SensorService API will not need to change.
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

    s_bma_ready = true;

    return SENSOR_SERVICE_OK;
}

sensor_service_status_t SensorService_ReadBmaSample(
    sensor_service_accel_sample_t *sample)
{
    bma456_motion_status_t status;

    if (sample == NULL)
    {
        return SENSOR_SERVICE_ERROR_PARAM;
    }

    if (!s_bma_ready)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return SENSOR_SERVICE_ERROR_BMA_READ;
    }

    status = BMA456Motion_ReadAccelMg(
        &sample->x_mg,
        &sample->y_mg,
        &sample->z_mg);

    if (status != BMA456_MOTION_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return SENSOR_SERVICE_ERROR_BMA_READ;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);

    return SENSOR_SERVICE_OK;
}

bool SensorService_IsBmaReady(void)
{
    return s_bma_ready && BMA456Motion_IsReady();
}
