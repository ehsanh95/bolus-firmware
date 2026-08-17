#include "fault_manager.h"
#include "bolus_led.h"

void FaultManager_Init(void)
{
    BolusLed_Init();
}

void FaultManager_Report(bolus_fault_domain_t fault)
{
    switch (fault)
    {
        case BOLUS_FAULT_SENSOR:
            BolusLed_On(BOLUS_LED_SENSOR);
            break;

        case BOLUS_FAULT_MCU:
            BolusLed_On(BOLUS_LED_MCU);
            break;

        case BOLUS_FAULT_RF:
            BolusLed_On(BOLUS_LED_RF);
            break;

        default:
            break;
    }
}

void FaultManager_Clear(bolus_fault_domain_t fault)
{
    switch (fault)
    {
        case BOLUS_FAULT_SENSOR:
            BolusLed_Off(BOLUS_LED_SENSOR);
            break;

        case BOLUS_FAULT_MCU:
            BolusLed_Off(BOLUS_LED_MCU);
            break;

        case BOLUS_FAULT_RF:
            BolusLed_Off(BOLUS_LED_RF);
            break;

        default:
            break;
    }
}

void FaultManager_ClearAll(void)
{
    BolusLed_AllOff();
}
