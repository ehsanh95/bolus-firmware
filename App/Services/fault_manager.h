#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#include "../Application/bolus_types.h"

/*
 * Detailed Phase 5 fault identifiers.
 *
 * The numeric value is also the bit position inside bolus_fault_mask_t.
 * Keep BOLUS_FAULT_COUNT <= 32 while the mask is uint32_t.
 */
typedef enum
{
    BOLUS_FAULT_TMP117_COMM = 0,
    BOLUS_FAULT_TMP117_TIMEOUT,
    BOLUS_FAULT_TMP117_INVALID_DATA,

    BOLUS_FAULT_BMA456_COMM,
    BOLUS_FAULT_BMA456_FIFO_OVERFLOW,
    BOLUS_FAULT_BMA456_STUCK,

    BOLUS_FAULT_MPU6050_COMM,
    BOLUS_FAULT_MPU6050_INIT,
    BOLUS_FAULT_MPU6050_TIMEOUT,

    BOLUS_FAULT_BATTERY_MEASUREMENT,
    BOLUS_FAULT_BATTERY_LOW,
    BOLUS_FAULT_BATTERY_CRITICAL,

    BOLUS_FAULT_RF_COMM,
    BOLUS_FAULT_RF_TX_TIMEOUT,

    BOLUS_FAULT_CONFIG_INVALID,
    BOLUS_FAULT_POWER_CONTROL,
    BOLUS_FAULT_SYSTEM_INTERNAL,

    BOLUS_FAULT_COUNT
} bolus_fault_id_t;

#define BOLUS_FAULT_CODE_NONE  0xFFU
#define BOLUS_FAULT_BIT(id)     (1UL << (uint32_t)(id))

typedef enum
{
    BOLUS_FAULT_SEVERITY_INFO = 0,
    BOLUS_FAULT_SEVERITY_WARNING,
    BOLUS_FAULT_SEVERITY_CRITICAL
} bolus_fault_severity_t;

typedef enum
{
    BOLUS_FAULT_DOMAIN_SENSOR = 0,
    BOLUS_FAULT_DOMAIN_BATTERY,
    BOLUS_FAULT_DOMAIN_RF,
    BOLUS_FAULT_DOMAIN_POWER,
    BOLUS_FAULT_DOMAIN_CONFIG,
    BOLUS_FAULT_DOMAIN_SYSTEM
} bolus_fault_domain_detail_t;

typedef struct
{
    bolus_fault_domain_detail_t domain;
    bolus_fault_severity_t severity;
    bool latched;
} bolus_fault_descriptor_t;

void FaultManager_Init(void);

/* New Phase 5 API. */
void FaultManager_Raise(bolus_fault_id_t fault);
bool FaultManager_ClearFault(bolus_fault_id_t fault);
bool FaultManager_IsActive(bolus_fault_id_t fault);

bolus_fault_mask_t FaultManager_GetActiveMask(void);
bolus_fault_mask_t FaultManager_GetHistoryMask(void);
uint8_t FaultManager_GetLastFaultCode(void);
bolus_health_status_t FaultManager_GetHealth(void);

bool FaultManager_GetDescriptor(
    bolus_fault_id_t fault,
    bolus_fault_descriptor_t *descriptor);

/*
 * History is RAM-only for now. Persistent history can be added in the NVM
 * stage without changing the telemetry contract.
 */
void FaultManager_ClearHistory(void);

/*
 * Legacy Phase 4 diagnostic API kept temporarily so main.c can remain a known
 * good bring-up harness while Phase 5 services are introduced incrementally.
 */
typedef enum
{
    BOLUS_FAULT_SENSOR = 0,
    BOLUS_FAULT_MCU,
    BOLUS_FAULT_RF
} bolus_fault_domain_t;

void FaultManager_Report(bolus_fault_domain_t fault);
void FaultManager_Clear(bolus_fault_domain_t fault);
void FaultManager_ClearAll(void);

#endif /* FAULT_MANAGER_H */
