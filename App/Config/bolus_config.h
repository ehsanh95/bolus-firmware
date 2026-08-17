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

#endif