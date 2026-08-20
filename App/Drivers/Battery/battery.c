#include "battery.h"
#include "bolus_power.h"
#include "bolus_config.h"

#include <stddef.h>


static ADC_HandleTypeDef *battery_adc = NULL;


/*
 * Approximate Li-ion voltage -> SOC table.
 *
 * This is only for initial firmware bring-up.
 * Later replace this table with measured discharge data
 * from the real Bolus battery.
 */
typedef struct
{
    uint16_t mv;
    uint8_t percent;

} battery_soc_point_t;


static const battery_soc_point_t battery_soc_table[] =
{
    {4200U, 100U},
    {4100U,  90U},
    {4000U,  80U},
    {3900U,  65U},
    {3800U,  50U},
    {3700U,  30U},
    {3600U,  15U},
    {3500U,   7U},
    {3400U,   3U},
    {3300U,   0U}
};


/* ---------------------------------------------------------- */

static uint8_t Battery_EstimatePercent(uint16_t battery_mv)
{
    const uint32_t count =
        sizeof(battery_soc_table) /
        sizeof(battery_soc_table[0]);


    /*
     * Above highest table voltage.
     */
    if (battery_mv >= battery_soc_table[0].mv)
    {
        return 100U;
    }


    /*
     * Below lowest table voltage.
     */
    if (battery_mv <= battery_soc_table[count - 1U].mv)
    {
        return 0U;
    }


    /*
     * Find the two surrounding voltage points
     * and linearly interpolate between them.
     */
    for (uint32_t i = 0; i < (count - 1U); i++)
    {
        uint16_t v_high = battery_soc_table[i].mv;
        uint16_t v_low  = battery_soc_table[i + 1U].mv;

        if ((battery_mv <= v_high) &&
            (battery_mv >= v_low))
        {
            uint8_t p_high = battery_soc_table[i].percent;
            uint8_t p_low  = battery_soc_table[i + 1U].percent;

            uint32_t numerator =
                (uint32_t)(battery_mv - v_low) *
                (uint32_t)(p_high - p_low);

            uint32_t denominator =
                (uint32_t)(v_high - v_low);

            return (uint8_t)
                (p_low + (numerator / denominator));
        }
    }


    return 0U;
}


/* ---------------------------------------------------------- */

battery_status_t Battery_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL)
    {
        return BATTERY_ERROR_PARAM;
    }


    battery_adc = hadc;


    /*
     * ADC calibration.
     */
    if (HAL_ADCEx_Calibration_Start(battery_adc,
                                    ADC_SINGLE_ENDED) != HAL_OK)
    {
        battery_adc = NULL;

        return BATTERY_ERROR_CALIBRATION;
    }


    return BATTERY_OK;
}


/* ---------------------------------------------------------- */

battery_status_t Battery_ReadRaw(uint16_t *raw)
{
    battery_status_t status = BATTERY_OK;


    if ((battery_adc == NULL) ||
        (raw == NULL))
    {
        return BATTERY_ERROR_PARAM;
    }


    /*
     * Enable battery resistor divider.
     */
    BolusPower_On(BOLUS_POWER_SOC);


    /*
     * Allow divider and ADC input capacitor
     * to settle.
     */
    HAL_Delay(BOLUS_BATTERY_SETTLE_MS);


    /*
     * Start ADC.
     */
    if (HAL_ADC_Start(battery_adc) != HAL_OK)
    {
        status = BATTERY_ERROR_START;

        goto cleanup;
    }


    /*
     * Wait for ADC conversion.
     */
    if (HAL_ADC_PollForConversion(
            battery_adc,
            BOLUS_ADC_TIMEOUT_MS) != HAL_OK)
    {
        status = BATTERY_ERROR_TIMEOUT;

        goto stop_adc;
    }


    /*
     * Read raw 12-bit value.
     */
    *raw =
        (uint16_t)HAL_ADC_GetValue(battery_adc);


stop_adc:

    HAL_ADC_Stop(battery_adc);


cleanup:

    /*
     * Important:
     * divider must not remain powered.
     */
    BolusPower_Off(BOLUS_POWER_SOC);


    return status;
}


/* ---------------------------------------------------------- */

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


    /*
     * Convert 12-bit ADC value to millivolts.
     *
     * ADC:
     * 0    -> 0 mV
     * 4095 -> VDDA
     */
    *mv =
        (uint16_t)
        (
            (
                ((uint32_t)raw *
                 BOLUS_ADC_VREF_MV)
                + 2047U
            )
            / 4095U
        );


    return BATTERY_OK;
}


/* ---------------------------------------------------------- */

battery_status_t Battery_ReadMillivolts(uint16_t *mv)
{
    uint16_t adc_mv;

    uint32_t battery_mv;

    battery_status_t status;


    if (mv == NULL)
    {
        return BATTERY_ERROR_PARAM;
    }


    status =
        Battery_ReadAdcMillivolts(&adc_mv);


    if (status != BATTERY_OK)
    {
        return status;
    }


    /*
     * Divider equation:
     *
     * Vbat =
     * Vadc *
     * (Rtop + Rbottom)
     * ----------------
     *      Rbottom
     */
    battery_mv =
        (
            (uint32_t)adc_mv *
            (
                BOLUS_BATTERY_DIVIDER_TOP_OHM +
                BOLUS_BATTERY_DIVIDER_BOTTOM_OHM
            )
        )
        /
        BOLUS_BATTERY_DIVIDER_BOTTOM_OHM;


    /*
     * uint16_t is more than enough
     * for a single-cell battery voltage.
     */
    *mv = (uint16_t)battery_mv;


    return BATTERY_OK;
}


/* ---------------------------------------------------------- */

battery_status_t Battery_ReadPercent(uint8_t *percent)
{
    uint16_t battery_mv;

    battery_status_t status;


    if (percent == NULL)
    {
        return BATTERY_ERROR_PARAM;
    }


    status =
        Battery_ReadMillivolts(&battery_mv);


    if (status != BATTERY_OK)
    {
        return status;
    }


    *percent =
        Battery_EstimatePercent(battery_mv);


    return BATTERY_OK;
}
