#ifndef BOLUS_CONFIG_H
#define BOLUS_CONFIG_H

#include "main.h"

/*
 * BOLUS PROJECT MODIFICATION
 *
 * Required by RADIO_MEMCPY8 and RADIO_MEMSET8
 * used by the SX1276 driver.
 */
#include <string.h>

/*
 * ============================================================
 * RFM95W / SX1276 CONFIGURATION
 * ============================================================
 *
 * Bolus project configuration for the RFM95W radio module.
 *
 * The RFM95W is based on the Semtech SX1276 transceiver.
 * ST middleware-specific configuration dependencies are not
 * used in this project.
 * ============================================================
 */

/*
 * ------------------------------------------------------------
 * Device identification
 * ------------------------------------------------------------
 *
 * SX1276 RegVersion address = 0x42
 * Expected value           = 0x12
 */
#define RFM95W_REG_VERSION                 0x42U
#define RFM95W_EXPECTED_VERSION            0x12U


/*
 * ------------------------------------------------------------
 * SPI interface
 * ------------------------------------------------------------
 */
#define RFM95W_SPI_TIMEOUT_MS              100U


/*
 * ------------------------------------------------------------
 * Power and reset timing
 * ------------------------------------------------------------
 */
#define RFM95W_POWERUP_DELAY_MS            10U

#define RFM95W_RESET_ASSERT_TIME_MS        1U
#define RFM95W_RESET_RELEASE_TIME_MS       6U


/*
 * ------------------------------------------------------------
 * Default RF configuration
 * ------------------------------------------------------------
 *
 * Initial values for Phase 4 bring-up.
 * These may be changed later according to the final radio
 * protocol and regional frequency plan.
 */
#define RFM95W_DEFAULT_FREQUENCY_HZ        868000000UL

#define RFM95W_DEFAULT_TX_POWER_DBM        10

/*
 * Semtech LoRa bandwidth index:
 *
 * 0 = 125 kHz
 * 1 = 250 kHz
 * 2 = 500 kHz
 */
#define RFM95W_DEFAULT_BANDWIDTH           0U

#define RFM95W_DEFAULT_SPREADING_FACTOR    7U

/*
 * Coding rate:
 *
 * 1 = 4/5
 * 2 = 4/6
 * 3 = 4/7
 * 4 = 4/8
 */
#define RFM95W_DEFAULT_CODING_RATE         1U

#define RFM95W_DEFAULT_PREAMBLE_LENGTH     8U


/*
 * ------------------------------------------------------------
 * ST SX1276 middleware adaptation
 * ------------------------------------------------------------
 *
 * The original ST radio_conf.h maps these functions to the
 * STM32 middleware utility layer.
 *
 * Bolus uses STM32 HAL and standard C library directly.
 */

/* Delay interface */
#define RADIO_DELAY_MS(ms)                 HAL_Delay((ms))

/* Memory set interface */
#define RADIO_MEMSET8(dest, value, size)   \
    memset((dest), (value), (size))

/* Memory copy interface */
#define RADIO_MEMCPY8(dest, src, size)     \
    memcpy((dest), (src), (size))



/*
 * ------------------------------------------------------------
 * end
 * ------------------------------------------------------------
 *
 */







/*
 * Power gate polarity
 * Current project documentation indicates the P-MOS power gates
 * are generally active-low.
 *
 * Verify individual rails against schematic before final freeze.
 */

#define BOLUS_TMP_PWR_ACTIVE_STATE      GPIO_PIN_RESET
#define BOLUS_MPU_PWR_ACTIVE_STATE      GPIO_PIN_RESET
#define BOLUS_BMA_PWR_ACTIVE_STATE      GPIO_PIN_RESET
#define BOLUS_RFM_PWR_ACTIVE_STATE      GPIO_PIN_RESET
#define BOLUS_SOC_PWR_ACTIVE_STATE      GPIO_PIN_RESET

/* ---------- Battery ADC ---------- */


#define BOLUS_BATTERY_SETTLE_MS            5U
#define BOLUS_ADC_TIMEOUT_MS               20U

/*
 * Temporary nominal VDDA.
 * Later we can use VREFINT for better accuracy.
 */
#define BOLUS_ADC_VREF_MV                  3300U


/*
 * Battery voltage divider.
 *
 * Temporary assumption:
 * R_TOP = R_BOTTOM
 *
 * Therefore:
 * Vbattery = Vadc * 2
 *
 * Replace these values after schematic verification.
 */
#define BOLUS_BATTERY_DIVIDER_TOP_OHM      100000U
#define BOLUS_BATTERY_DIVIDER_BOTTOM_OHM   100000U

/* ============================================================
 * TMP117 Configuration
 * ============================================================ */

/*
 * Default operating mode.
 *
 * TMP117_CC_MODE = Continuous conversion
 * TMP117_SD_MODE = Shutdown
 * TMP117_OS_MODE = One-shot
 *
 * For Bolus we start in shutdown mode to reduce power.
 */
#define TMP117_DEFAULT_MODE             TMP117_SD_MODE


/*
 * Conversion cycle.
 *
 * With no averaging, 15.5 ms is enough for the
 * fastest temperature conversion.
 */
#define TMP117_CONVERSION_CYCLE         TMP117_C15mS5


/*
 * ALERT pin polarity.
 */
#define TMP117_ALERT_POLARITY           TMP117_POL_H


/*
 * Averaging.
 *
 * For initial bring-up:
 * no averaging = fastest response.
 */
#define TMP117_DEFAULT_AVG_MODE         TMP117_NO_AVG


/*
 * ALERT pin function.
 *
 * DATA mode:
 * ALERT indicates Data Ready.
 */
#define TMP117_DEFAULT_ALERT_MODE       TMP117_DATA_MODE


/*
 * Optional temperature offset correction.
 */
#define TMP117_USE_OFFSET               0
#define TMP117_OFFSET_CORRECTION        0.0


/*
 * Optional high temperature threshold.
 */
#define TMP117_ALERT_HIGH_ENABLE        0
#define TMP117_HIGH_LIMIT_C             50.0


/*
 * Optional low temperature threshold.
 */
#define TMP117_ALERT_LOW_ENABLE         0
#define TMP117_LOW_LIMIT_C              0.0


/*
 * Maximum time allowed for an I2C transaction.
 *
 * Never use HAL_MAX_DELAY in the driver.
 */
#define TMP117_I2C_TIMEOUT_MS           100U


/*
 * Delay after sensor initialization.
 */
#define TMP117_INIT_DELAY_MS            100U
/* ============================================================
 * MPU6050 Configuration
 * ============================================================ */

/*
 * I2C address
 * AD0 = GND -> 7-bit address = 0x68
 * STM32 HAL expects shifted address.
 */
#define MPU6050_I2C_ADDRESS             (0x68U << 1)

/*
 * Maximum I2C transaction time.
 */
#define MPU6050_I2C_TIMEOUT_MS          100U


/*
 * Accelerometer full-scale range:
 * 0 = ±2g
 * 1 = ±4g
 * 2 = ±8g
 * 3 = ±16g
 */
#define MPU6050_ACCEL_FS_SEL            0U

#if MPU6050_ACCEL_FS_SEL == 0
    #define MPU6050_ACCEL_SCALE         16384.0f
#elif MPU6050_ACCEL_FS_SEL == 1
    #define MPU6050_ACCEL_SCALE         8192.0f
#elif MPU6050_ACCEL_FS_SEL == 2
    #define MPU6050_ACCEL_SCALE         4096.0f
#elif MPU6050_ACCEL_FS_SEL == 3
    #define MPU6050_ACCEL_SCALE         2048.0f
#endif


/*
 * Existing Z-axis calibration factor
 * from the previous project.
 *
 * Keep for now; recalibration can be done later.
 */
#define MPU6050_ACCEL_Z_CORRECTOR       14418.0f


/*
 * Gyroscope full-scale range:
 * 0 = ±250 deg/s
 * 1 = ±500 deg/s
 * 2 = ±1000 deg/s
 * 3 = ±2000 deg/s
 */
#define MPU6050_GYRO_FS_SEL             0U

#if MPU6050_GYRO_FS_SEL == 0
    #define MPU6050_GYRO_SCALE          131.0f
#elif MPU6050_GYRO_FS_SEL == 1
    #define MPU6050_GYRO_SCALE          65.5f
#elif MPU6050_GYRO_FS_SEL == 2
    #define MPU6050_GYRO_SCALE          32.8f
#elif MPU6050_GYRO_FS_SEL == 3
    #define MPU6050_GYRO_SCALE          16.4f
#endif


/*
 * Sample Rate Divider
 *
 * With 1 kHz base rate:
 * 1 kHz / (1 + 7) = 125 Hz
 */
#define MPU6050_SAMPLE_DIVIDER          0x07U


/*
 * Digital Low-Pass Filter.
 * Previous project used 0x03.
 */
#define MPU6050_DLPF_CFG                0x03U


/*
 * Low-power behavior.
 * Keep MPU6050 asleep between measurements.
 */
#define MPU6050_SLEEP_BETWEEN_SAMPLES   1


/*
 * Kalman processing from legacy library.
 * We keep the capability for now.
 */
#define USE_KALMAN_FILTER               1
#endif /* BOLUS_CONFIG_H */
