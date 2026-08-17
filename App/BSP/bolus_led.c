#include "bolus_led.h"
#include "main.h"

static void BolusLed_Write(bolus_led_t led, GPIO_PinState state)
{
    switch (led)
    {
        case BOLUS_LED_SENSOR:
            HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, state);
            break;

        case BOLUS_LED_MCU:
            HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, state);
            break;

        case BOLUS_LED_RF:
            HAL_GPIO_WritePin(LED3_GPIO_Port, LED3_Pin, state);
            break;

        default:
            break;
    }
}

void BolusLed_Init(void)
{
    BolusLed_AllOff();
}

void BolusLed_On(bolus_led_t led)
{
    BolusLed_Write(led, GPIO_PIN_SET);
}

void BolusLed_Off(bolus_led_t led)
{
    BolusLed_Write(led, GPIO_PIN_RESET);
}

void BolusLed_Set(bolus_led_t led, bool on)
{
    if (on)
    {
        BolusLed_On(led);
    }
    else
    {
        BolusLed_Off(led);
    }
}

void BolusLed_AllOff(void)
{
    BolusLed_Off(BOLUS_LED_SENSOR);
    BolusLed_Off(BOLUS_LED_MCU);
    BolusLed_Off(BOLUS_LED_RF);
}
