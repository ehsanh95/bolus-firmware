#include "bolus_led.h"
#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)
#include "main.h"
#endif

static void BolusLed_Write(bolus_led_t led, bool on)
{
#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)
    GPIO_PinState state = on ? GPIO_PIN_SET : GPIO_PIN_RESET;

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
#else
    (void)led;
    (void)on;
#endif
}

void BolusLed_Init(void)
{
    BolusLed_AllOff();
}

void BolusLed_On(bolus_led_t led)
{
    BolusLed_Write(led, true);
}

void BolusLed_Off(bolus_led_t led)
{
    BolusLed_Write(led, false);
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
