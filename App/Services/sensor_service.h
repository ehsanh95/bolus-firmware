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
    SENSOR_SERVICE_ERROR_TMP_INVALID_DATA,
    SENSOR_SERVICE_ERROR_MPU_INIT,
    SENSOR_SERVICE_ERROR_MPU_READ
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

typedef struct
{
    int16_t accel_x_mg;
    int16_t accel_y_mg;
    int16_t accel_z_mg;

    int32_t gyro_x_mdps;
    int32_t gyro_y_mdps;
    int32_t gyro_z_mdps;

    int32_t temperature_mdeg_c;

    uint16_t sample_rate_hz;
    uint8_t accel_range_g;
    uint16_t gyro_range_dps;
} sensor_service_mpu_sample_t;

/*
 * Compact features from one power-gated MPU6050 burst. Raw burst samples are
 * deliberately discarded after online accumulation in normal operation.
 */
typedef struct
{
    uint16_t configured_duration_ms;
    uint16_t sample_count;

    uint16_t peak_dynamic_accel_mg;
    uint16_t rms_dynamic_accel_mg;

    uint16_t peak_angular_velocity_dps;
    uint16_t rms_angular_velocity_dps;
    uint32_t total_angular_motion_cdeg;

    bool orientation_change_valid;
    int16_t roll_change_cdeg;
    int16_t pitch_change_cdeg;
    uint16_t orientation_change_cdeg;

    uint16_t sample_rate_hz;
    uint8_t accel_range_g;
    uint16_t gyro_range_dps;
} sensor_service_mpu_burst_features_t;

sensor_service_status_t SensorService_InitBma(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadBmaSample(
    sensor_service_bma_sample_t *sample);

bool SensorService_IsBmaReady(void);

sensor_service_status_t SensorService_InitTemperature(
    I2C_HandleTypeDef *hi2c,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadTemperatureOneShot(
    sensor_service_temperature_sample_t *sample);

bool SensorService_IsTemperatureReady(void);

/*
 * MPU6050 high-detail motion path.
 *
 * Phase-5 policy:
 * - normally physically power-gated OFF;
 * - init verifies the configured path once, then powers it OFF;
 * - SensorService_ReadMpuBurst powers/configures once per accepted event pulse,
 *   keeps the device awake for the bounded burst, extracts features online,
 *   sleeps it, and powers the rail OFF on both success and failure paths;
 * - raw burst samples are not retained in normal mode.
 *
 * STAGING NOTE: burst path is untested on hardware as of 2026-08-30.
 */
sensor_service_status_t SensorService_InitMpu(
    I2C_HandleTypeDef *hi2c,
    const bolus_runtime_config_t *config);

sensor_service_status_t SensorService_ReadMpuSample(
    sensor_service_mpu_sample_t *sample);

sensor_service_status_t SensorService_ReadMpuBurst(
    sensor_service_mpu_burst_features_t *features);

bool SensorService_IsMpuReady(void);

#endif /* SENSOR_SERVICE_H */
