#include "mpu6050_motion.h"

#include "bolus_config.h"

#include <math.h>

#define MPU6050_MOTION_WHO_AM_I_REG       0x75U
#define MPU6050_MOTION_PWR_MGMT_1_REG     0x6BU
#define MPU6050_MOTION_SMPLRT_DIV_REG     0x19U
#define MPU6050_MOTION_CONFIG_REG         0x1AU
#define MPU6050_MOTION_GYRO_CONFIG_REG    0x1BU
#define MPU6050_MOTION_ACCEL_CONFIG_REG   0x1CU
#define MPU6050_MOTION_ACCEL_XOUT_H_REG   0x3BU

#define MPU6050_MOTION_WHO_AM_I_VALUE     0x68U
#define MPU6050_MOTION_DLPF_CFG           0x03U
#define MPU6050_MOTION_GYRO_SETTLE_MS     30U
#define MPU6050_MOTION_RAD_TO_DEG         57.29577951308232

/*
 * Phase-5 orientation bench diagnostics.
 *
 * Roll/pitch/tilt are derived from the gravity vector, so they are meaningful
 * when the bolus is stationary or moving slowly enough that linear acceleration
 * is small compared with 1 g.  Absolute yaw/compass heading is intentionally
 * marked unavailable: MPU6050 has no magnetometer, so gravity alone cannot
 * observe rotation about the vertical axis.
 */
bool mpu6050_diag_orientation_valid = false;
int32_t mpu6050_diag_roll_mdeg = 0;
int32_t mpu6050_diag_pitch_mdeg = 0;
int32_t mpu6050_diag_tilt_mdeg = 0;
bool mpu6050_diag_absolute_yaw_available = false;

static I2C_HandleTypeDef *s_hi2c = NULL;
static bool s_ready = false;
static uint16_t s_accel_lsb_per_g = 8192U;
static uint16_t s_gyro_lsb_per_dps_x10 = 655U;

static int32_t DegreesToMilliDegrees(double degrees)
{
    double scaled = degrees * 1000.0;

    if (scaled >= 0.0)
    {
        return (int32_t)(scaled + 0.5);
    }

    return (int32_t)(scaled - 0.5);
}

static void UpdateOrientationDiagnostics(
    int16_t accel_x_mg,
    int16_t accel_y_mg,
    int16_t accel_z_mg)
{
    double ax = (double)accel_x_mg;
    double ay = (double)accel_y_mg;
    double az = (double)accel_z_mg;
    double norm_mg = sqrt((ax * ax) + (ay * ay) + (az * az));
    double roll_deg;
    double pitch_deg;
    double tilt_deg;

    mpu6050_diag_orientation_valid =
        ((norm_mg >= 700.0) && (norm_mg <= 1300.0));

    if (norm_mg < 1.0)
    {
        mpu6050_diag_roll_mdeg = 0;
        mpu6050_diag_pitch_mdeg = 0;
        mpu6050_diag_tilt_mdeg = 0;
        mpu6050_diag_orientation_valid = false;
        return;
    }

    /*
     * Board-frame convention:
     *   roll  : rotation about +X
     *   pitch : rotation about +Y
     *   tilt  : angle between +Z and the gravity/acceleration vector
     *
     * These signs are intentionally bench-validated before they become a
     * telemetry contract because final enclosure PCB orientation may require
     * an axis remap.
     */
    roll_deg = atan2(ay, az) * MPU6050_MOTION_RAD_TO_DEG;
    pitch_deg = atan2(-ax, sqrt((ay * ay) + (az * az))) *
                MPU6050_MOTION_RAD_TO_DEG;
    tilt_deg = atan2(sqrt((ax * ax) + (ay * ay)), az) *
               MPU6050_MOTION_RAD_TO_DEG;

    mpu6050_diag_roll_mdeg = DegreesToMilliDegrees(roll_deg);
    mpu6050_diag_pitch_mdeg = DegreesToMilliDegrees(pitch_deg);
    mpu6050_diag_tilt_mdeg = DegreesToMilliDegrees(tilt_deg);
}

static bool WriteReg(uint8_t reg, uint8_t value)
{
    return (HAL_I2C_Mem_Write(
                s_hi2c,
                MPU6050_I2C_ADDRESS,
                reg,
                I2C_MEMADD_SIZE_8BIT,
                &value,
                1U,
                MPU6050_I2C_TIMEOUT_MS) == HAL_OK);
}

static bool ReadReg(uint8_t reg, uint8_t *value)
{
    if (value == NULL)
    {
        return false;
    }

    return (HAL_I2C_Mem_Read(
                s_hi2c,
                MPU6050_I2C_ADDRESS,
                reg,
                I2C_MEMADD_SIZE_8BIT,
                value,
                1U,
                MPU6050_I2C_TIMEOUT_MS) == HAL_OK);
}

static bool MapAccelRange(
    uint8_t range_g,
    uint8_t *fs_sel,
    uint16_t *lsb_per_g)
{
    if ((fs_sel == NULL) || (lsb_per_g == NULL))
    {
        return false;
    }

    switch (range_g)
    {
        case 2U:
            *fs_sel = 0U;
            *lsb_per_g = 16384U;
            return true;

        case 4U:
            *fs_sel = 1U;
            *lsb_per_g = 8192U;
            return true;

        case 8U:
            *fs_sel = 2U;
            *lsb_per_g = 4096U;
            return true;

        case 16U:
            *fs_sel = 3U;
            *lsb_per_g = 2048U;
            return true;

        default:
            return false;
    }
}

static bool MapGyroRange(
    uint16_t range_dps,
    uint8_t *fs_sel,
    uint16_t *lsb_per_dps_x10)
{
    if ((fs_sel == NULL) || (lsb_per_dps_x10 == NULL))
    {
        return false;
    }

    switch (range_dps)
    {
        case 250U:
            *fs_sel = 0U;
            *lsb_per_dps_x10 = 1310U;
            return true;

        case 500U:
            *fs_sel = 1U;
            *lsb_per_dps_x10 = 655U;
            return true;

        case 1000U:
            *fs_sel = 2U;
            *lsb_per_dps_x10 = 328U;
            return true;

        case 2000U:
            *fs_sel = 3U;
            *lsb_per_dps_x10 = 164U;
            return true;

        default:
            return false;
    }
}

static bool MapSampleRate(uint16_t sample_rate_hz, uint8_t *divider)
{
    uint16_t divider_value;

    if (divider == NULL)
    {
        return false;
    }

    /* DLPF enabled => internal gyro output rate is 1 kHz. */
    if ((sample_rate_hz == 0U) ||
        (sample_rate_hz > 1000U) ||
        ((1000U % sample_rate_hz) != 0U))
    {
        return false;
    }

    divider_value = (uint16_t)((1000U / sample_rate_hz) - 1U);

    if (divider_value > 255U)
    {
        return false;
    }

    *divider = (uint8_t)divider_value;
    return true;
}

mpu6050_motion_status_t MPU6050Motion_Sleep(void)
{
    uint8_t pwr1 = 0U;

    if ((s_hi2c == NULL) || (!s_ready))
    {
        return MPU6050_MOTION_ERROR_NOT_READY;
    }

    if (!ReadReg(MPU6050_MOTION_PWR_MGMT_1_REG, &pwr1))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    pwr1 |= 0x40U;

    if (!WriteReg(MPU6050_MOTION_PWR_MGMT_1_REG, pwr1))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    return MPU6050_MOTION_OK;
}

mpu6050_motion_status_t MPU6050Motion_Init(
    I2C_HandleTypeDef *hi2c,
    const mpu6050_motion_config_t *config)
{
    uint8_t who_am_i = 0U;
    uint8_t sample_divider = 0U;
    uint8_t accel_fs_sel = 0U;
    uint8_t gyro_fs_sel = 0U;
    uint16_t accel_lsb_per_g = 0U;
    uint16_t gyro_lsb_per_dps_x10 = 0U;

    if ((hi2c == NULL) || (config == NULL))
    {
        return MPU6050_MOTION_ERROR_PARAM;
    }

    if ((!MapSampleRate(config->sample_rate_hz, &sample_divider)) ||
        (!MapAccelRange(
            config->accel_range_g,
            &accel_fs_sel,
            &accel_lsb_per_g)) ||
        (!MapGyroRange(
            config->gyro_range_dps,
            &gyro_fs_sel,
            &gyro_lsb_per_dps_x10)))
    {
        return MPU6050_MOTION_ERROR_CONFIG;
    }

    s_hi2c = hi2c;
    s_ready = false;

    if (!ReadReg(MPU6050_MOTION_WHO_AM_I_REG, &who_am_i))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    if (who_am_i != MPU6050_MOTION_WHO_AM_I_VALUE)
    {
        return MPU6050_MOTION_ERROR_CONFIG;
    }

    /* Wake using the internal oscillator for the initial bounded bench path. */
    if (!WriteReg(MPU6050_MOTION_PWR_MGMT_1_REG, 0x00U))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    if (!WriteReg(MPU6050_MOTION_CONFIG_REG, MPU6050_MOTION_DLPF_CFG))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    if (!WriteReg(MPU6050_MOTION_SMPLRT_DIV_REG, sample_divider))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    if (!WriteReg(
            MPU6050_MOTION_ACCEL_CONFIG_REG,
            (uint8_t)(accel_fs_sel << 3)))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    if (!WriteReg(
            MPU6050_MOTION_GYRO_CONFIG_REG,
            (uint8_t)(gyro_fs_sel << 3)))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    s_accel_lsb_per_g = accel_lsb_per_g;
    s_gyro_lsb_per_dps_x10 = gyro_lsb_per_dps_x10;
    s_ready = true;

    if (MPU6050Motion_Sleep() != MPU6050_MOTION_OK)
    {
        s_ready = false;
        return MPU6050_MOTION_ERROR_COMM;
    }

    return MPU6050_MOTION_OK;
}

mpu6050_motion_status_t MPU6050Motion_ReadSample(
    mpu6050_motion_sample_t *sample)
{
    uint8_t pwr1 = 0U;
    uint8_t data[14] = {0};
    int16_t accel_x_raw;
    int16_t accel_y_raw;
    int16_t accel_z_raw;
    int16_t temp_raw;
    int16_t gyro_x_raw;
    int16_t gyro_y_raw;
    int16_t gyro_z_raw;
    HAL_StatusTypeDef hal_status;

    if (sample == NULL)
    {
        return MPU6050_MOTION_ERROR_PARAM;
    }

    if ((s_hi2c == NULL) || (!s_ready))
    {
        return MPU6050_MOTION_ERROR_NOT_READY;
    }

    if (!ReadReg(MPU6050_MOTION_PWR_MGMT_1_REG, &pwr1))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    pwr1 &= (uint8_t)~0x40U;

    if (!WriteReg(MPU6050_MOTION_PWR_MGMT_1_REG, pwr1))
    {
        return MPU6050_MOTION_ERROR_COMM;
    }

    HAL_Delay(MPU6050_MOTION_GYRO_SETTLE_MS);

    hal_status = HAL_I2C_Mem_Read(
        s_hi2c,
        MPU6050_I2C_ADDRESS,
        MPU6050_MOTION_ACCEL_XOUT_H_REG,
        I2C_MEMADD_SIZE_8BIT,
        data,
        sizeof(data),
        MPU6050_I2C_TIMEOUT_MS);

    /* Always attempt software sleep after the acquisition window. */
    (void)MPU6050Motion_Sleep();

    if (hal_status != HAL_OK)
    {
        mpu6050_diag_orientation_valid = false;
        return MPU6050_MOTION_ERROR_COMM;
    }

    accel_x_raw = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
    accel_y_raw = (int16_t)(((uint16_t)data[2] << 8) | data[3]);
    accel_z_raw = (int16_t)(((uint16_t)data[4] << 8) | data[5]);
    temp_raw = (int16_t)(((uint16_t)data[6] << 8) | data[7]);
    gyro_x_raw = (int16_t)(((uint16_t)data[8] << 8) | data[9]);
    gyro_y_raw = (int16_t)(((uint16_t)data[10] << 8) | data[11]);
    gyro_z_raw = (int16_t)(((uint16_t)data[12] << 8) | data[13]);

    sample->accel_x_mg =
        (int16_t)(((int32_t)accel_x_raw * 1000L) / s_accel_lsb_per_g);
    sample->accel_y_mg =
        (int16_t)(((int32_t)accel_y_raw * 1000L) / s_accel_lsb_per_g);
    sample->accel_z_mg =
        (int16_t)(((int32_t)accel_z_raw * 1000L) / s_accel_lsb_per_g);

    sample->gyro_x_mdps =
        ((int32_t)gyro_x_raw * 10000L) / s_gyro_lsb_per_dps_x10;
    sample->gyro_y_mdps =
        ((int32_t)gyro_y_raw * 10000L) / s_gyro_lsb_per_dps_x10;
    sample->gyro_z_mdps =
        ((int32_t)gyro_z_raw * 10000L) / s_gyro_lsb_per_dps_x10;

    sample->temperature_mdeg_c =
        (((int32_t)temp_raw * 1000L) / 340L) + 36530L;

    UpdateOrientationDiagnostics(
        sample->accel_x_mg,
        sample->accel_y_mg,
        sample->accel_z_mg);

    return MPU6050_MOTION_OK;
}

bool MPU6050Motion_IsReady(void)
{
    return s_ready;
}
