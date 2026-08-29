#ifndef MPU6050_MOTION_H
#define MPU6050_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"

typedef enum
{
    MPU6050_MOTION_OK = 0,
    MPU6050_MOTION_ERROR_PARAM,
    MPU6050_MOTION_ERROR_CONFIG,
    MPU6050_MOTION_ERROR_COMM,
    MPU6050_MOTION_ERROR_NOT_READY
} mpu6050_motion_status_t;

typedef struct
{
    uint16_t sample_rate_hz;
    uint8_t accel_range_g;
    uint16_t gyro_range_dps;
} mpu6050_motion_config_t;

typedef struct
{
    int16_t accel_x_mg;
    int16_t accel_y_mg;
    int16_t accel_z_mg;

    int32_t gyro_x_mdps;
    int32_t gyro_y_mdps;
    int32_t gyro_z_mdps;

    int32_t temperature_mdeg_c;
} mpu6050_motion_sample_t;

/*
 * Configure and verify one powered MPU6050 instance.
 * The rail must already be ON. The function leaves the device in SLEEP.
 */
mpu6050_motion_status_t MPU6050Motion_Init(
    I2C_HandleTypeDef *hi2c,
    const mpu6050_motion_config_t *config);

/*
 * Wake, allow the gyro path to stabilize, acquire one bounded 14-byte sample,
 * then return the device to SLEEP. The rail remains under SensorService control.
 */
mpu6050_motion_status_t MPU6050Motion_ReadSample(
    mpu6050_motion_sample_t *sample);

/* Explicit software sleep before physical rail-off. */
mpu6050_motion_status_t MPU6050Motion_Sleep(void);

bool MPU6050Motion_IsReady(void);

#endif /* MPU6050_MOTION_H */
