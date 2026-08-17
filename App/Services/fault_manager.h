#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

typedef enum
{
    BOLUS_FAULT_SENSOR = 0,
    BOLUS_FAULT_MCU,
    BOLUS_FAULT_RF
} bolus_fault_domain_t;

void FaultManager_Init(void);

void FaultManager_Report(bolus_fault_domain_t fault);
void FaultManager_Clear(bolus_fault_domain_t fault);

void FaultManager_ClearAll(void);

#endif
