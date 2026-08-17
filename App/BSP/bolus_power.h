#ifndef BOLUS_POWER_H
#define BOLUS_POWER_H

#include <stdbool.h>

typedef enum
{
    BOLUS_POWER_TMP117 = 0,
    BOLUS_POWER_MPU6050,
    BOLUS_POWER_BMA456,
    BOLUS_POWER_RFM95W,
    BOLUS_POWER_SOC
} bolus_power_domain_t;

void BolusPower_Init(void);

void BolusPower_On(bolus_power_domain_t domain);
void BolusPower_Off(bolus_power_domain_t domain);

bool BolusPower_IsOn(bolus_power_domain_t domain);

void BolusPower_AllOff(void);

#endif