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
    SENSOR_SERVICE_ERROR_BMA_READ,
    SENSOR_SERVICE_ERROR_TMP_INIT,
    SENSOR_SERVICE_ERROR_TMP_READ,
    SENSOR_SERVICE_ERROR_TMP_TIMEOUT,
    SENSOR_SERVICE_ERROR_TMP_INVALID_DATA
} sensor_service_status_t;

typedef struct
{
    int16_t x_mg;
    int16_t y_mg;
    int16_t z_mg;

    uint32_t step_total;
    uint32_t step_delta;
    bolus_bma_step_sensitivity_t step_sensitivity;
} sensor_service_bma_sample_t;

typedef struct
{
    int32_t temperature_mdeg_c;
    uint16_t device_id;
    uint8_t averaging_samples;
} sensor_service_temperature_sample_t;

/* BMA456 low-power continuous motion path. */
sensor_service_status_t SensorService_InitBma(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadBmaSample(
    sensor_service_bma_sample_t *sample);

bool SensorService_IsBmaReady(void);

/*
 * TMP117 precision-temperature path.
 *
 * Phase-5 policy:
 * - rail is enabled during initialization and left powered for now;
 * - TMP117 remains in shutdown between measurements;
 * - each acquisition is a bounded one-shot conversion;
 * - rail-off vs shutdown-only leakage is characterized later in PowerService.
 */
sensor_service_status_t SensorService_InitTemperature(
    I2C_HandleTypeDef *hi2c,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadTemperatureOneShot(
    sensor_service_temperature_sample_t *sample);

bool SensorService_IsTemperatureReady(void);

#endif /* SENSOR_SERVICE_H */
