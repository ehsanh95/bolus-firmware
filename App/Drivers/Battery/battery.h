#ifndef BATTERY_H
#define BATTERY_H

#include "stm32l4xx_hal.h"
#include <stdint.h>


typedef enum
{
    BATTERY_OK = 0,
    BATTERY_ERROR_PARAM,
    BATTERY_ERROR_CALIBRATION,
    BATTERY_ERROR_START,
    BATTERY_ERROR_TIMEOUT

} battery_status_t;


/*
 * Initialize battery ADC driver
 * and perform ADC calibration.
 */
battery_status_t Battery_Init(ADC_HandleTypeDef *hadc);


/*
 * Read raw 12-bit ADC value.
 */
battery_status_t Battery_ReadRaw(uint16_t *raw);


/*
 * Read voltage directly on SOC_ADC pin.
 *
 * Example:
 * ADC pin = 1900 mV
 */
battery_status_t Battery_ReadAdcMillivolts(uint16_t *mv);


/*
 * Read estimated real battery voltage
 * after compensating for resistor divider.
 *
 * Example:
 * Battery = 3800 mV
 */
battery_status_t Battery_ReadMillivolts(uint16_t *mv);


/*
 * Read estimated battery state of charge.
 *
 * Output:
 * 0 ... 100 %
 */
battery_status_t Battery_ReadPercent(uint8_t *percent);


#endif /* BATTERY_H */
