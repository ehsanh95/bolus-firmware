#ifndef BOLUS_POWER_H
#define BOLUS_POWER_H

#include <stdbool.h>

typedef enum
{
    BOLUS_POWER_TMP117 = 0,
    BOLUS_POWER_MPU6050,
    BOLUS_POWER_BMA456,
    BOLUS_POWER_RFM95W,
    BOLUS_POWER_SOC,

    BOLUS_POWER_COUNT
} bolus_power_domain_t;


/*
 * Initialize controllable load power domains.
 *
 * NOTE:
 * This does NOT control:
 * - Main_Reg_PWR_ON
 * - MCU_BCK_PWR_ON
 *
 * Regulator sequencing will be handled separately.
 */
void BolusPower_Init(void);


/* Turn one power domain ON/OFF. */
void BolusPower_On(bolus_power_domain_t domain);
void BolusPower_Off(bolus_power_domain_t domain);


/* Toggle one domain between ON and OFF. */
void BolusPower_Toggle(bolus_power_domain_t domain);


/* Return true if the requested power domain is ON. */
bool BolusPower_IsOn(bolus_power_domain_t domain);


/* Turn all controllable load domains OFF. */
void BolusPower_AllOff(void);

#endif /* BOLUS_POWER_H */
