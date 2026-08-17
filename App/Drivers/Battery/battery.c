#include "battery.h"
#include "bolus_power.h"
#include "bolus_config.h"

static ADC_HandleTypeDef *battery_adc = NULL;

battery_status_t Battery_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL)
    {
        return BATTERY_ERROR_PARAM;
    }

    battery_adc = hadc;

    if (HAL_ADCEx_Calibration_Start(battery_adc,
                                   ADC_SINGLE_ENDED) != HAL_OK)
    {
        return BATTERY_ERROR_CALIBRATION;
    }

    return BATTERY_OK;
}

battery_status_t Battery_ReadRaw(uint16_t *raw)
{
    battery_status_t status = BATTERY_OK;

    if ((battery_adc == NULL) || (raw == NULL))
    {
        return BATTERY_ERROR_PARAM;
    }

    BolusPower_On(BOLUS_POWER_SOC);

    HAL_Delay(BOLUS_BATTERY_SETTLE_MS);

    if (HAL_ADC_Start(battery_adc) != HAL_OK)
    {
        status = BATTERY_ERROR_START;
        goto cleanup;
    }

    if (HAL_ADC_PollForConversion(battery_adc,
                                  BOLUS_ADC_TIMEOUT_MS) != HAL_OK)
    {
        status = BATTERY_ERROR_TIMEOUT;
        goto stop_adc;
    }

    *raw = (uint16_t)HAL_ADC_GetValue(battery_adc);

stop_adc:

    HAL_ADC_Stop(battery_adc);

cleanup:

    BolusPower_Off(BOLUS_POWER_SOC);

    return status;
}

battery_status_t Battery_ReadAdcMillivolts(uint16_t *mv)
{
    uint16_t raw;
    battery_status_t status;

    if (mv == NULL)
    {
        return BATTERY_ERROR_PARAM;
    }

    status = Battery_ReadRaw(&raw);

    if (status != BATTERY_OK)
    {
        return status;
    }

    *mv = (uint16_t)
        ((((uint32_t)raw * BOLUS_ADC_VREF_MV) + 2047U) / 4095U);

    return BATTERY_OK;
}
