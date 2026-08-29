#include "bolus_power.h"
#include "bolus_config.h"
#include "main.h"


/*
 * Temporary Phase-5 diagnostic timings.
 *
 * The board currently uses very weak external gate pull-ups. During reset the
 * MCU pins can briefly transition through Hi-Z before CubeMX drives the gates
 * to their OFF levels. Give every switched rail time to reach a definite OFF
 * state before bring-up, and give each rail extra time to settle after ON.
 *
 * These delays are deliberately conservative for diagnosis only. Once the
 * power-gate hardware is characterized they should be reduced/removed and the
 * final timings owned by PowerService.
 */
#define BOLUS_POWER_DIAG_ALL_OFF_SETTLE_MS   250U
#define BOLUS_POWER_DIAG_RAIL_ON_SETTLE_MS    50U


static bool BolusPower_IsValidDomain(bolus_power_domain_t domain)
{
    return ((domain >= BOLUS_POWER_TMP117) &&
            (domain < BOLUS_POWER_COUNT));
}


static void BolusPower_Write(GPIO_TypeDef *port,
                             uint16_t pin,
                             GPIO_PinState active_state,
                             bool on)
{
    GPIO_PinState output_state;

    if (on)
    {
        output_state = active_state;
    }
    else
    {
        output_state = (active_state == GPIO_PIN_SET)
                     ? GPIO_PIN_RESET
                     : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(port, pin, output_state);
}


void BolusPower_Init(void)
{
    /*
     * Safe default:
     * all controllable loads OFF.
     *
     * Main regulator and backup regulator are intentionally
     * NOT touched here.
     */
    BolusPower_AllOff();

    /*
     * Diagnostic only: allow weak gate pull-ups, MOSFET gates and downstream
     * rail capacitance to settle completely before any sensor is powered.
     */
    HAL_Delay(BOLUS_POWER_DIAG_ALL_OFF_SETTLE_MS);
}


void BolusPower_On(bolus_power_domain_t domain)
{
    if (!BolusPower_IsValidDomain(domain))
    {
        return;
    }

    switch (domain)
    {
        case BOLUS_POWER_TMP117:

            BolusPower_Write(TMP_PWR_ON_GPIO_Port,
                             TMP_PWR_ON_Pin,
                             BOLUS_TMP_PWR_ACTIVE_STATE,
                             true);
            break;


        case BOLUS_POWER_MPU6050:

            BolusPower_Write(MPU_PWR_ON_GPIO_Port,
                             MPU_PWR_ON_Pin,
                             BOLUS_MPU_PWR_ACTIVE_STATE,
                             true);
            break;


        case BOLUS_POWER_BMA456:

            BolusPower_Write(PEDO_PWR_GPIO_Port,
                             PEDO_PWR_Pin,
                             BOLUS_BMA_PWR_ACTIVE_STATE,
                             true);
            break;


        case BOLUS_POWER_RFM95W:

            BolusPower_Write(RFM_PWR_ON_GPIO_Port,
                             RFM_PWR_ON_Pin,
                             BOLUS_RFM_PWR_ACTIVE_STATE,
                             true);
            break;


        case BOLUS_POWER_SOC:

            BolusPower_Write(SOC_CHK_ON_GPIO_Port,
                             SOC_CHK_ON_Pin,
                             BOLUS_SOC_PWR_ACTIVE_STATE,
                             true);
            break;


        default:
            break;
    }

    /*
     * Diagnostic only: do not access a newly enabled peripheral immediately.
     * Existing device-specific delays still remain in their services, so this
     * intentionally exaggerates settling time for the current bench test.
     */
    HAL_Delay(BOLUS_POWER_DIAG_RAIL_ON_SETTLE_MS);
}


void BolusPower_Off(bolus_power_domain_t domain)
{
    if (!BolusPower_IsValidDomain(domain))
    {
        return;
    }

    switch (domain)
    {
        case BOLUS_POWER_TMP117:

            BolusPower_Write(TMP_PWR_ON_GPIO_Port,
                             TMP_PWR_ON_Pin,
                             BOLUS_TMP_PWR_ACTIVE_STATE,
                             false);
            break;


        case BOLUS_POWER_MPU6050:

            BolusPower_Write(MPU_PWR_ON_GPIO_Port,
                             MPU_PWR_ON_Pin,
                             BOLUS_MPU_PWR_ACTIVE_STATE,
                             false);
            break;


        case BOLUS_POWER_BMA456:

            BolusPower_Write(PEDO_PWR_GPIO_Port,
                             PEDO_PWR_Pin,
                             BOLUS_BMA_PWR_ACTIVE_STATE,
                             false);
            break;


        case BOLUS_POWER_RFM95W:

            BolusPower_Write(RFM_PWR_ON_GPIO_Port,
                             RFM_PWR_ON_Pin,
                             BOLUS_RFM_PWR_ACTIVE_STATE,
                             false);
            break;


        case BOLUS_POWER_SOC:

            BolusPower_Write(SOC_CHK_ON_GPIO_Port,
                             SOC_CHK_ON_Pin,
                             BOLUS_SOC_PWR_ACTIVE_STATE,
                             false);
            break;


        default:
            break;
    }
}


bool BolusPower_IsOn(bolus_power_domain_t domain)
{
    GPIO_PinState current_state;
    GPIO_PinState active_state;

    if (!BolusPower_IsValidDomain(domain))
    {
        return false;
    }

    switch (domain)
    {
        case BOLUS_POWER_TMP117:

            current_state =
                HAL_GPIO_ReadPin(TMP_PWR_ON_GPIO_Port,
                                 TMP_PWR_ON_Pin);

            active_state = BOLUS_TMP_PWR_ACTIVE_STATE;
            break;


        case BOLUS_POWER_MPU6050:

            current_state =
                HAL_GPIO_ReadPin(MPU_PWR_ON_GPIO_Port,
                                 MPU_PWR_ON_Pin);

            active_state = BOLUS_MPU_PWR_ACTIVE_STATE;
            break;


        case BOLUS_POWER_BMA456:

            current_state =
                HAL_GPIO_ReadPin(PEDO_PWR_GPIO_Port,
                                 PEDO_PWR_Pin);

            active_state = BOLUS_BMA_PWR_ACTIVE_STATE;
            break;


        case BOLUS_POWER_RFM95W:

            current_state =
                HAL_GPIO_ReadPin(RFM_PWR_ON_GPIO_Port,
                                 RFM_PWR_ON_Pin);

            active_state = BOLUS_RFM_PWR_ACTIVE_STATE;
            break;


        case BOLUS_POWER_SOC:

            current_state =
                HAL_GPIO_ReadPin(SOC_CHK_ON_GPIO_Port,
                                 SOC_CHK_ON_Pin);

            active_state = BOLUS_SOC_PWR_ACTIVE_STATE;
            break;


        default:
            return false;
    }

    return (current_state == active_state);
}


void BolusPower_Toggle(bolus_power_domain_t domain)
{
    if (!BolusPower_IsValidDomain(domain))
    {
        return;
    }

    if (BolusPower_IsOn(domain))
    {
        BolusPower_Off(domain);
    }
    else
    {
        BolusPower_On(domain);
    }
}


void BolusPower_AllOff(void)
{
    BolusPower_Off(BOLUS_POWER_TMP117);
    BolusPower_Off(BOLUS_POWER_MPU6050);
    BolusPower_Off(BOLUS_POWER_BMA456);
    BolusPower_Off(BOLUS_POWER_RFM95W);
    BolusPower_Off(BOLUS_POWER_SOC);
}
