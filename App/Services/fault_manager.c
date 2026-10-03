#include "fault_manager.h"
#include "bolus_led.h"

#include <stddef.h>
#include <string.h>

static bolus_fault_mask_t active_faults = 0U;
static bolus_fault_mask_t fault_history = 0U;
static uint8_t last_fault_code = BOLUS_FAULT_CODE_NONE;

volatile fault_manager_diag_t fault_manager_diag = {0};

static bool FaultManager_IsValidId(bolus_fault_id_t fault)
{
    return ((uint32_t)fault < (uint32_t)BOLUS_FAULT_COUNT);
}

static bolus_fault_descriptor_t FaultManager_Describe(bolus_fault_id_t fault)
{
    bolus_fault_descriptor_t descriptor;

    descriptor.domain = BOLUS_FAULT_DOMAIN_SYSTEM;
    descriptor.severity = BOLUS_FAULT_SEVERITY_WARNING;
    descriptor.latched = false;

    switch (fault)
    {
        case BOLUS_FAULT_TMP117_COMM:
        case BOLUS_FAULT_TMP117_TIMEOUT:
        case BOLUS_FAULT_TMP117_INVALID_DATA:
        case BOLUS_FAULT_BMA456_COMM:
        case BOLUS_FAULT_BMA456_FIFO_OVERFLOW:
        case BOLUS_FAULT_BMA456_STUCK:
        case BOLUS_FAULT_MPU6050_COMM:
        case BOLUS_FAULT_MPU6050_INIT:
        case BOLUS_FAULT_MPU6050_TIMEOUT:
            descriptor.domain = BOLUS_FAULT_DOMAIN_SENSOR;
            descriptor.severity = BOLUS_FAULT_SEVERITY_WARNING;
            break;

        case BOLUS_FAULT_BATTERY_MEASUREMENT:
        case BOLUS_FAULT_BATTERY_LOW:
            descriptor.domain = BOLUS_FAULT_DOMAIN_BATTERY;
            descriptor.severity = BOLUS_FAULT_SEVERITY_WARNING;
            break;

        case BOLUS_FAULT_BATTERY_CRITICAL:
            descriptor.domain = BOLUS_FAULT_DOMAIN_BATTERY;
            descriptor.severity = BOLUS_FAULT_SEVERITY_CRITICAL;
            break;

        case BOLUS_FAULT_RF_COMM:
        case BOLUS_FAULT_RF_TX_TIMEOUT:
            descriptor.domain = BOLUS_FAULT_DOMAIN_RF;
            descriptor.severity = BOLUS_FAULT_SEVERITY_WARNING;
            break;

        case BOLUS_FAULT_CONFIG_INVALID:
            descriptor.domain = BOLUS_FAULT_DOMAIN_CONFIG;
            descriptor.severity = BOLUS_FAULT_SEVERITY_WARNING;
            break;

        case BOLUS_FAULT_POWER_CONTROL:
            descriptor.domain = BOLUS_FAULT_DOMAIN_POWER;
            descriptor.severity = BOLUS_FAULT_SEVERITY_CRITICAL;
            descriptor.latched = true;
            break;

        case BOLUS_FAULT_SYSTEM_INTERNAL:
            descriptor.domain = BOLUS_FAULT_DOMAIN_SYSTEM;
            descriptor.severity = BOLUS_FAULT_SEVERITY_CRITICAL;
            descriptor.latched = true;
            break;

        default:
            break;
    }

    return descriptor;
}

static bool FaultManager_DomainHasActiveFault(bolus_fault_domain_detail_t domain)
{
    for (uint32_t i = 0U; i < (uint32_t)BOLUS_FAULT_COUNT; i++)
    {
        bolus_fault_id_t id = (bolus_fault_id_t)i;

        if ((active_faults & BOLUS_FAULT_BIT(id)) != 0U)
        {
            if (FaultManager_Describe(id).domain == domain)
            {
                return true;
            }
        }
    }

    return false;
}

static void FaultManager_UpdateLedForDomain(bolus_fault_domain_detail_t domain)
{
#if (BOLUS_BENCH_LED_DIAGNOSTICS != 0)
    /* The main-loop bench renderer exclusively owns GPIO while enabled.
     * Raise/Clear can run during LoRa callbacks; never toggle inside RX1/RX2. */
    (void)domain;
    return;
#else
    /*
     * Shared physical LEDs: clearing sensor fault must not hide battery fault.
     * Each update checks ALL domains mapped to the given LED.
     */
    bool active;
    switch (domain)
    {
        case BOLUS_FAULT_DOMAIN_SENSOR:
        case BOLUS_FAULT_DOMAIN_BATTERY:
            active = FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_SENSOR) ||
                     FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_BATTERY);
            break;
        case BOLUS_FAULT_DOMAIN_RF:
            active = FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_RF);
            break;
        default:
            active = FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_POWER) ||
                     FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_CONFIG) ||
                     FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_SYSTEM);
            break;
    }

    switch (domain)
    {
        case BOLUS_FAULT_DOMAIN_SENSOR:
        case BOLUS_FAULT_DOMAIN_BATTERY:
            if (active)
            {
                BolusLed_On(BOLUS_LED_SENSOR);
            }
            else
            {
                BolusLed_Off(BOLUS_LED_SENSOR);
            }
            break;

        case BOLUS_FAULT_DOMAIN_RF:
            if (active)
            {
                BolusLed_On(BOLUS_LED_RF);
            }
            else
            {
                BolusLed_Off(BOLUS_LED_RF);
            }
            break;

        case BOLUS_FAULT_DOMAIN_POWER:
        case BOLUS_FAULT_DOMAIN_CONFIG:
        case BOLUS_FAULT_DOMAIN_SYSTEM:
        default:
            if (active)
            {
                BolusLed_On(BOLUS_LED_MCU);
            }
            else
            {
                BolusLed_Off(BOLUS_LED_MCU);
            }
            break;
    }
#endif
}

void FaultManager_Init(void)
{
    active_faults = 0U;
    fault_history = 0U;
    last_fault_code = BOLUS_FAULT_CODE_NONE;
    memset((void *)&fault_manager_diag, 0, sizeof(fault_manager_diag));
    fault_manager_diag.last_raised_id = BOLUS_FAULT_CODE_NONE;
    fault_manager_diag.last_cleared_id = BOLUS_FAULT_CODE_NONE;

    BolusLed_Init();
    BolusLed_AllOff();
}

void FaultManager_Raise(bolus_fault_id_t fault)
{
    bolus_fault_descriptor_t descriptor;

    if (!FaultManager_IsValidId(fault))
    {
        return;
    }

    fault_manager_diag.raise_call_count++;
    if ((active_faults & BOLUS_FAULT_BIT(fault)) == 0U)
        fault_manager_diag.newly_active_count++;
    else
        fault_manager_diag.repeat_raise_count++;
    if (fault_manager_diag.raises_by_id[fault] != UINT16_MAX)
        fault_manager_diag.raises_by_id[fault]++;

    active_faults |= BOLUS_FAULT_BIT(fault);
    fault_history |= BOLUS_FAULT_BIT(fault);
    fault_manager_diag.active_mask = active_faults;
    fault_manager_diag.history_mask = fault_history;
    fault_manager_diag.last_raised_id = (uint8_t)fault;
    last_fault_code = (uint8_t)fault;

    descriptor = FaultManager_Describe(fault);
    FaultManager_UpdateLedForDomain(descriptor.domain);
}

bool FaultManager_ClearFault(bolus_fault_id_t fault)
{
    bolus_fault_descriptor_t descriptor;

    if (!FaultManager_IsValidId(fault))
    {
        return false;
    }

    descriptor = FaultManager_Describe(fault);

    if (descriptor.latched)
    {
        return false;
    }

    if ((active_faults & BOLUS_FAULT_BIT(fault)) != 0U)
    {
        fault_manager_diag.clear_count++;
        fault_manager_diag.last_cleared_id = (uint8_t)fault;
    }
    active_faults &= ~BOLUS_FAULT_BIT(fault);
    fault_manager_diag.active_mask = active_faults;
    FaultManager_UpdateLedForDomain(descriptor.domain);

    return true;
}

bool FaultManager_IsActive(bolus_fault_id_t fault)
{
    if (!FaultManager_IsValidId(fault))
    {
        return false;
    }

    return ((active_faults & BOLUS_FAULT_BIT(fault)) != 0U);
}

bolus_fault_mask_t FaultManager_GetActiveMask(void)
{
    return active_faults;
}

bolus_fault_mask_t FaultManager_GetHistoryMask(void)
{
    return fault_history;
}

uint8_t FaultManager_GetLastFaultCode(void)
{
    return last_fault_code;
}

bolus_health_status_t FaultManager_GetHealth(void)
{
    bool any_active = false;

    for (uint32_t i = 0U; i < (uint32_t)BOLUS_FAULT_COUNT; i++)
    {
        bolus_fault_id_t id = (bolus_fault_id_t)i;

        if ((active_faults & BOLUS_FAULT_BIT(id)) == 0U)
        {
            continue;
        }

        any_active = true;

        if (FaultManager_Describe(id).severity ==
            BOLUS_FAULT_SEVERITY_CRITICAL)
        {
            return BOLUS_HEALTH_CRITICAL;
        }
    }

    if (any_active)
    {
        return BOLUS_HEALTH_DEGRADED;
    }

    return BOLUS_HEALTH_OK;
}

bool FaultManager_GetDescriptor(
    bolus_fault_id_t fault,
    bolus_fault_descriptor_t *descriptor)
{
    if ((!FaultManager_IsValidId(fault)) ||
        (descriptor == NULL))
    {
        return false;
    }

    *descriptor = FaultManager_Describe(fault);
    return true;
}

void FaultManager_ClearHistory(void)
{
    fault_history = 0U;
    fault_manager_diag.history_mask = 0U;
}

/* -------------------------------------------------------------------------
 * Legacy Phase 4 diagnostic compatibility.
 * ------------------------------------------------------------------------- */

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
            if (!FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_SENSOR) &&
                !FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_BATTERY))
            {
                BolusLed_Off(BOLUS_LED_SENSOR);
            }
            break;

        case BOLUS_FAULT_MCU:
            if (!FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_POWER) &&
                !FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_CONFIG) &&
                !FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_SYSTEM))
            {
                BolusLed_Off(BOLUS_LED_MCU);
            }
            break;

        case BOLUS_FAULT_RF:
            if (!FaultManager_DomainHasActiveFault(BOLUS_FAULT_DOMAIN_RF))
            {
                BolusLed_Off(BOLUS_LED_RF);
            }
            break;

        default:
            break;
    }
}

void FaultManager_ClearAll(void)
{
    /* Explicit lab reset; ClearFault() still respects latched fault policy. */
    active_faults = 0U;
    fault_manager_diag.active_mask = 0U;
    BolusLed_AllOff();
}
