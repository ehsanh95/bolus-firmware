#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"
#include "../Config/bolus_runtime_config.h"

typedef enum
{
    SENSOR_SERVICE_OK = 0,
    SENSOR_SERVICE_ERROR_PARAM,
    SENSOR_SERVICE_ERROR_CONFIG,
    SENSOR_SERVICE_ERROR_BMA_INIT,
    SENSOR_SERVICE_ERROR_BMA_READ
} sensor_service_status_t;

typedef struct
{
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;
} sensor_service_accel_sample_t;

/*
 * Phase 5 incremental BMA entry point.
 * TMP117, MPU6050 and Battery are added behind this same service later.
 */
sensor_service_status_t SensorService_InitBma(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadBmaSample(
    sensor_service_accel_sample_t *sample);

bool SensorService_IsBmaReady(void);

#endif /* SENSOR_SERVICE_H */
