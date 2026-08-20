#ifndef BOLUS_CONFIG_H
#define BOLUS_CONFIG_H

#include "main.h"

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

#endif
