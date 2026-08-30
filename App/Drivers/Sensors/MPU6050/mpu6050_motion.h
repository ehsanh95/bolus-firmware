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

/* Phase-5 orientation diagnostics derived from the most recent accel sample. */
extern bool mpu6050_diag_orientation_valid;
extern int32_t mpu6050_diag_roll_mdeg;
extern int32_t mpu6050_diag_pitch_mdeg;
extern int32_t mpu6050_diag_tilt_mdeg;
extern bool mpu6050_diag_absolute_yaw_available;

/*
 * Configure and verify one powered MPU6050 instance.
 * The rail must already be ON. The function leaves the device in SLEEP.
 */
mpu6050_motion_status_t MPU6050Motion_Init(
    I2C_HandleTypeDef *hi2c,
    const mpu6050_motion_config_t *config);

/*
 * Single-sample compatibility path: wake, stabilize, read once, then sleep.
 */
mpu6050_motion_status_t MPU6050Motion_ReadSample(
    mpu6050_motion_sample_t *sample);

/*
 * Event-burst primitives. BeginBurst wakes/stabilizes once, ReadBurstSample
 * performs one 14-byte accel/gyro/temp read without sleeping, and EndBurst
 * returns the device to software sleep. Physical rail gating remains owned by
 * the caller/SensorService.
 *
 * STAGING NOTE: introduced for Phase-5 event integration and not yet verified
 * on hardware as of 2026-08-30.
 */
mpu6050_motion_status_t MPU6050Motion_BeginBurst(void);
mpu6050_motion_status_t MPU6050Motion_ReadBurstSample(
    mpu6050_motion_sample_t *sample);
mpu6050_motion_status_t MPU6050Motion_EndBurst(void);

/* Explicit software sleep before physical rail-off. */
mpu6050_motion_status_t MPU6050Motion_Sleep(void);

bool MPU6050Motion_IsReady(void);
bool MPU6050Motion_IsBurstActive(void);

#endif /* MPU6050_MOTION_H */
