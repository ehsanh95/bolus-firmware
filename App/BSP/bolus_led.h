#ifndef BOLUS_LED_H
#define BOLUS_LED_H

#include <stdbool.h>

/* Debug on; Release off. Set -DBOLUS_BENCH_LED_DIAGNOSTICS=0 for power tests. */
#ifndef BOLUS_BENCH_LED_DIAGNOSTICS
# if defined(DEBUG)
#  define BOLUS_BENCH_LED_DIAGNOSTICS 1
# else
#  define BOLUS_BENCH_LED_DIAGNOSTICS 0
# endif
#endif

typedef enum
{
    BOLUS_LED_SENSOR = 0,
    BOLUS_LED_MCU,
    BOLUS_LED_RF
} bolus_led_t;

void BolusLed_Init(void);

void BolusLed_On(bolus_led_t led);
void BolusLed_Off(bolus_led_t led);
void BolusLed_Set(bolus_led_t led, bool on);

void BolusLed_AllOff(void);

#endif
