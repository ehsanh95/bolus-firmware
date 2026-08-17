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

battery_status_t Battery_Init(ADC_HandleTypeDef *hadc);

battery_status_t Battery_ReadRaw(uint16_t *raw);

battery_status_t Battery_ReadAdcMillivolts(uint16_t *mv);

#endif
