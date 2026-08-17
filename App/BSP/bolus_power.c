#include "bolus_power.h"
#include "bolus_config.h"
#include "main.h"

static void BolusPower_Write(GPIO_TypeDef *port,
                             uint16_t pin,
                             GPIO_PinState active_state,
                             bool on)
{
    GPIO_PinState state;

    if (on)
    {
        state = active_state;
    }
    else
    {
        state = (active_state == GPIO_PIN_SET)
                    ? GPIO_PIN_RESET
                    : GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(port, pin, state);
}

void BolusPower_Init(void)
{
    BolusPower_AllOff();
}

void BolusPower_On(bolus_power_domain_t domain)
{
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
}

void BolusPower_Off(bolus_power_domain_t domain)
{
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
    GPIO_PinState state;
    GPIO_PinState active_state;

    switch (domain)
    {
        case BOLUS_POWER_TMP117:
            state = HAL_GPIO_ReadPin(TMP_PWR_ON_GPIO_Port,
                                     TMP_PWR_ON_Pin);
            active_state = BOLUS_TMP_PWR_ACTIVE_STATE;
            break;

        case BOLUS_POWER_MPU6050:
            state = HAL_GPIO_ReadPin(MPU_PWR_ON_GPIO_Port,
                                     MPU_PWR_ON_Pin);
            active_state = BOLUS_MPU_PWR_ACTIVE_STATE;
            break;

        case BOLUS_POWER_BMA456:
            state = HAL_GPIO_ReadPin(PEDO_PWR_GPIO_Port,
                                     PEDO_PWR_Pin);
            active_state = BOLUS_BMA_PWR_ACTIVE_STATE;
            break;

        case BOLUS_POWER_RFM95W:
            state = HAL_GPIO_ReadPin(RFM_PWR_ON_GPIO_Port,
                                     RFM_PWR_ON_Pin);
            active_state = BOLUS_RFM_PWR_ACTIVE_STATE;
            break;

        case BOLUS_POWER_SOC:
            state = HAL_GPIO_ReadPin(SOC_CHK_ON_GPIO_Port,
                                     SOC_CHK_ON_Pin);
            active_state = BOLUS_SOC_PWR_ACTIVE_STATE;
            break;

        default:
            return false;
    }

    return (state == active_state);
}

void BolusPower_AllOff(void)
{
    BolusPower_Off(BOLUS_POWER_TMP117);
    BolusPower_Off(BOLUS_POWER_MPU6050);
    BolusPower_Off(BOLUS_POWER_BMA456);
    BolusPower_Off(BOLUS_POWER_RFM95W);
    BolusPower_Off(BOLUS_POWER_SOC);
}