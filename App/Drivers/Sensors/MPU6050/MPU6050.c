/*
 * mpu6050.c
 *
 *  Created on: Nov 13, 2019
 *      Author: Bulanov Konstantin
 *
 *  Contact information
 *  -------------------
 *
 * e-mail   :  leech001@gmail.com
 */

/*
 * |---------------------------------------------------------------------------------
 * | Copyright (C) Bulanov Konstantin,2021
 * |
 * | This program is free software: you can redistribute it and/or modify
 * | it under the terms of the GNU General Public License as published by
 * | the Free Software Foundation, either version 3 of the License, or
 * | any later version.
 * |
 * | This program is distributed in the hope that it will be useful,
 * | but WITHOUT ANY WARRANTY; without even the implied warranty of
 * | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * | GNU General Public License for more details.
 * |
 * | You should have received a copy of the GNU General Public License
 * | along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * |
 * | Kalman filter algorithm used from https://github.com/TKJElectronics/KalmanFilter
 * |---------------------------------------------------------------------------------
 */

#include "MPU6050.h"
#include "math.h"
#include "bolus_config.h"



#define RAD_TO_DEG 57.295779513082320876798154814105

#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1_REG 0x6B
#define SMPLRT_DIV_REG 0x19
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define TEMP_OUT_H_REG 0x41
#define GYRO_CONFIG_REG 0x1B
#define GYRO_XOUT_H_REG 0x43

#define CONFIG_REG        0x1A
#define PWR_MGMT_2_REG    0x6C


static uint32_t timer;

Kalman_t KalmanX = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f};

Kalman_t KalmanY = {
    .Q_angle = 0.001f,
    .Q_bias = 0.003f,
    .R_measure = 0.03f,
};

uint8_t MPU6050_Init(I2C_HandleTypeDef *I2Cx)
{
    uint8_t check = 0;
    uint8_t Data = 0;

    if (I2Cx == NULL)
    {
        return 1;
    }

    if (HAL_I2C_Mem_Read(I2Cx,
                         MPU6050_I2C_ADDRESS,
                         WHO_AM_I_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         &check,
                         1,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    if (check != 0x68U)
    {
        return 1;
    }

    Data = 0x00U;
    if (HAL_I2C_Mem_Write(I2Cx,
                          MPU6050_I2C_ADDRESS,
                          PWR_MGMT_1_REG,
                          I2C_MEMADD_SIZE_8BIT,
                          &Data,
                          1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    Data = MPU6050_DLPF_CFG;
    if (HAL_I2C_Mem_Write(I2Cx,
                          MPU6050_I2C_ADDRESS,
                          CONFIG_REG,
                          I2C_MEMADD_SIZE_8BIT,
                          &Data,
                          1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    Data = MPU6050_SAMPLE_DIVIDER;
    if (HAL_I2C_Mem_Write(I2Cx,
                          MPU6050_I2C_ADDRESS,
                          SMPLRT_DIV_REG,
                          I2C_MEMADD_SIZE_8BIT,
                          &Data,
                          1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    Data = (uint8_t)(MPU6050_ACCEL_FS_SEL << 3);
    if (HAL_I2C_Mem_Write(I2Cx,
                          MPU6050_I2C_ADDRESS,
                          ACCEL_CONFIG_REG,
                          I2C_MEMADD_SIZE_8BIT,
                          &Data,
                          1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    Data = (uint8_t)(MPU6050_GYRO_FS_SEL << 3);
    if (HAL_I2C_Mem_Write(I2Cx,
                          MPU6050_I2C_ADDRESS,
                          GYRO_CONFIG_REG,
                          I2C_MEMADD_SIZE_8BIT,
                          &Data,
                          1,
                          MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return 1;
    }

    timer = HAL_GetTick();

#if MPU6050_SLEEP_BETWEEN_SAMPLES
    MPU6050_Sleep(I2Cx);
#endif

    return 0;
}

void MPU6050_Read_Accel(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from ACCEL_XOUT_H register

    if ((I2Cx == NULL) || (DataStruct == NULL))
    {
        return;
    }

    if (HAL_I2C_Mem_Read(I2Cx,
                         MPU6050_I2C_ADDRESS,
                         ACCEL_XOUT_H_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         Rec_Data,
                         6,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into acceleration in 'g'
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 16384.0
         for more details check ACCEL_CONFIG Register              ****/

    DataStruct->Ax = DataStruct->Accel_X_RAW / MPU6050_ACCEL_SCALE;
    DataStruct->Ay = DataStruct->Accel_Y_RAW / MPU6050_ACCEL_SCALE;
    DataStruct->Az = DataStruct->Accel_Z_RAW / MPU6050_ACCEL_Z_CORRECTOR;
}

void MPU6050_Read_Gyro(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[6];

    // Read 6 BYTES of data starting from GYRO_XOUT_H register

    if ((I2Cx == NULL) || (DataStruct == NULL))
    {
        return;
    }

    if (HAL_I2C_Mem_Read(I2Cx,
                         MPU6050_I2C_ADDRESS,
                         GYRO_XOUT_H_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         Rec_Data,
                         6,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);

    /*** convert the RAW values into dps (�/s)
         we have to divide according to the Full scale value set in FS_SEL
         I have configured FS_SEL = 0. So I am dividing by 131.0
         for more details check GYRO_CONFIG Register              ****/

    DataStruct->Gx = DataStruct->Gyro_X_RAW / MPU6050_GYRO_SCALE;
    DataStruct->Gy = DataStruct->Gyro_Y_RAW / MPU6050_GYRO_SCALE;
    DataStruct->Gz = DataStruct->Gyro_Z_RAW / MPU6050_GYRO_SCALE;
}

void MPU6050_Read_Temp(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[2];
    int16_t temp;

    // Read 2 BYTES of data starting from TEMP_OUT_H_REG register

    if ((I2Cx == NULL) || (DataStruct == NULL))
    {
        return;
    }

    if (HAL_I2C_Mem_Read(I2Cx,
                         MPU6050_I2C_ADDRESS,
                         TEMP_OUT_H_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         Rec_Data,
                         2,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    temp = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Temperature = (float)((int16_t)temp / (float)340.0 + (float)36.53);
}

void MPU6050_Read_All(I2C_HandleTypeDef *I2Cx, MPU6050_t *DataStruct)
{
    uint8_t Rec_Data[14];
    int16_t temp;

    // Read 14 BYTES of data starting from ACCEL_XOUT_H register

    if ((I2Cx == NULL) || (DataStruct == NULL))
    {
        return;
    }

    if (HAL_I2C_Mem_Read(I2Cx,
                         MPU6050_I2C_ADDRESS,
                         ACCEL_XOUT_H_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         Rec_Data,
                         14,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    DataStruct->Accel_X_RAW = (int16_t)(Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t)(Rec_Data[2] << 8 | Rec_Data[3]);
    DataStruct->Accel_Z_RAW = (int16_t)(Rec_Data[4] << 8 | Rec_Data[5]);
    temp = (int16_t)(Rec_Data[6] << 8 | Rec_Data[7]);
    DataStruct->Gyro_X_RAW = (int16_t)(Rec_Data[8] << 8 | Rec_Data[9]);
    DataStruct->Gyro_Y_RAW = (int16_t)(Rec_Data[10] << 8 | Rec_Data[11]);
    DataStruct->Gyro_Z_RAW = (int16_t)(Rec_Data[12] << 8 | Rec_Data[13]);

    DataStruct->Ax = DataStruct->Accel_X_RAW / MPU6050_ACCEL_SCALE;
    DataStruct->Ay = DataStruct->Accel_Y_RAW / MPU6050_ACCEL_SCALE;
    DataStruct->Az = DataStruct->Accel_Z_RAW / MPU6050_ACCEL_Z_CORRECTOR;
    DataStruct->Temperature = (float)((int16_t)temp / (float)340.0 + (float)36.53);
    DataStruct->Gx = DataStruct->Gyro_X_RAW / MPU6050_GYRO_SCALE;
    DataStruct->Gy = DataStruct->Gyro_Y_RAW / MPU6050_GYRO_SCALE;
    DataStruct->Gz = DataStruct->Gyro_Z_RAW / MPU6050_GYRO_SCALE;

#if USE_KALMAN_FILTER
    // Kalman angle solve
    double dt = (double)(HAL_GetTick() - timer) / 1000;
    timer = HAL_GetTick();
    double roll;
    double roll_sqrt = sqrt(
        DataStruct->Accel_X_RAW * DataStruct->Accel_X_RAW + DataStruct->Accel_Z_RAW * DataStruct->Accel_Z_RAW);
    if (roll_sqrt != 0.0)
    {
        roll = atan(DataStruct->Accel_Y_RAW / roll_sqrt) * RAD_TO_DEG;
    }
    else
    {
        roll = 0.0;
    }
    double pitch = atan2(-DataStruct->Accel_X_RAW, DataStruct->Accel_Z_RAW) * RAD_TO_DEG;
    if ((pitch < -90 && DataStruct->KalmanAngleY > 90) || (pitch > 90 && DataStruct->KalmanAngleY < -90))
    {
        KalmanY.angle = pitch;
        DataStruct->KalmanAngleY = pitch;
    }
    else
    {
        DataStruct->KalmanAngleY = Kalman_getAngle(&KalmanY, pitch, DataStruct->Gy, dt);
    }
    if (fabs(DataStruct->KalmanAngleY) > 90)
        DataStruct->Gx = -DataStruct->Gx;
    DataStruct->KalmanAngleX = Kalman_getAngle(&KalmanX, roll, DataStruct->Gx, dt);
#endif
}

double Kalman_getAngle(Kalman_t *Kalman, double newAngle, double newRate, double dt)
{
    double rate = newRate - Kalman->bias;
    Kalman->angle += dt * rate;

    Kalman->P[0][0] += dt * (dt * Kalman->P[1][1] - Kalman->P[0][1] - Kalman->P[1][0] + Kalman->Q_angle);
    Kalman->P[0][1] -= dt * Kalman->P[1][1];
    Kalman->P[1][0] -= dt * Kalman->P[1][1];
    Kalman->P[1][1] += Kalman->Q_bias * dt;

    double S = Kalman->P[0][0] + Kalman->R_measure;
    double K[2];
    K[0] = Kalman->P[0][0] / S;
    K[1] = Kalman->P[1][0] / S;

    double y = newAngle - Kalman->angle;
    Kalman->angle += K[0] * y;
    Kalman->bias += K[1] * y;

    double P00_temp = Kalman->P[0][0];
    double P01_temp = Kalman->P[0][1];

    Kalman->P[0][0] -= K[0] * P00_temp;
    Kalman->P[0][1] -= K[0] * P01_temp;
    Kalman->P[1][0] -= K[1] * P00_temp;
    Kalman->P[1][1] -= K[1] * P01_temp;

    return Kalman->angle;
};

/** Put the device into SLEEP (bit6 of PWR_MGMT_1). Typical current ~10 µA. */
void MPU6050_Sleep(I2C_HandleTypeDef *hi2c)
{
    uint8_t pwr1 = 0;

    if (hi2c == NULL)
    {
        return;
    }

    if (HAL_I2C_Mem_Read(hi2c,
                         MPU6050_I2C_ADDRESS,
                         PWR_MGMT_1_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         &pwr1,
                         1,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    pwr1 |= 0x40U;

    (void)HAL_I2C_Mem_Write(hi2c,
                            MPU6050_I2C_ADDRESS,
                            PWR_MGMT_1_REG,
                            I2C_MEMADD_SIZE_8BIT,
                            &pwr1,
                            1,
                            MPU6050_I2C_TIMEOUT_MS);
}

/** Clear SLEEP bit to wake the device. Leaves other bits untouched. */
void MPU6050_Wake(I2C_HandleTypeDef *hi2c)
{
    uint8_t pwr1 = 0;

    if (hi2c == NULL)
    {
        return;
    }

    if (HAL_I2C_Mem_Read(hi2c,
                         MPU6050_I2C_ADDRESS,
                         PWR_MGMT_1_REG,
                         I2C_MEMADD_SIZE_8BIT,
                         &pwr1,
                         1,
                         MPU6050_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return;
    }

    pwr1 &= (uint8_t)~0x40U;

    (void)HAL_I2C_Mem_Write(hi2c,
                            MPU6050_I2C_ADDRESS,
                            PWR_MGMT_1_REG,
                            I2C_MEMADD_SIZE_8BIT,
                            &pwr1,
                            1,
                            MPU6050_I2C_TIMEOUT_MS);
}

/** Wake + short stabilization delay + dummy gyro read to flush initial transient. */
void MPU6050_WakeAndStabilize(I2C_HandleTypeDef *hi2c)
{
    MPU6050_Wake(hi2c);

    // With DLPF enabled and moderate ODR, 2 ms is typically enough to settle.
    HAL_Delay(2);

    // Dummy read from gyro (6 bytes) to discard the first sample after wake
    uint8_t dump[6];
    (void)HAL_I2C_Mem_Read(hi2c, MPU6050_I2C_ADDRESS, GYRO_XOUT_H_REG, 1, dump, 6, MPU6050_I2C_TIMEOUT_MS);
}



