#ifndef BMA456_MOTION_H
#define BMA456_MOTION_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"

/*
 * Bolus-specific low-power motion wrapper for BMA456H.
 *
 * This wrapper intentionally lives beside the Bosch SensorAPI instead of
 * putting application policy inside the vendor driver. It exposes only the
 * capabilities required by the Bolus SensorService.
 */

typedef enum
{
    BMA456_MOTION_ODR_6_25_HZ = 0,
    BMA456_MOTION_ODR_12_5_HZ,
    BMA456_MOTION_ODR_25_HZ,
    BMA456_MOTION_ODR_50_HZ
} bma456_motion_odr_t;

typedef struct
{
    bma456_motion_odr_t odr;
    uint8_t range_g;
    uint8_t averaging_samples;
} bma456_motion_config_t;

typedef enum
{
    BMA456_MOTION_OK = 0,
    BMA456_MOTION_ERROR_PARAM,
    BMA456_MOTION_ERROR_CONFIG,
    BMA456_MOTION_ERROR_COMM,
    BMA456_MOTION_ERROR_NOT_READY
} bma456_motion_status_t;

/* The BMA456 power rail must be ON before initialization. */
bma456_motion_status_t BMA456Motion_Init(
    SPI_HandleTypeDef *hspi,
    const bma456_motion_config_t *config);

/* Read one XYZ acceleration sample converted to milli-g. */
bma456_motion_status_t BMA456Motion_ReadAccelMg(
    int16_t *x_mg,
    int16_t *y_mg,
    int16_t *z_mg);

bool BMA456Motion_IsReady(void);

#endif /* BMA456_MOTION_H */
