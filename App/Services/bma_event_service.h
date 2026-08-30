#ifndef BMA_EVENT_SERVICE_H
#define BMA_EVENT_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"
#include "../Config/bolus_runtime_config.h"

typedef enum
{
    BMA_EVENT_SERVICE_OK = 0,
    BMA_EVENT_SERVICE_ERROR_PARAM,
    BMA_EVENT_SERVICE_ERROR_CONFIG,
    BMA_EVENT_SERVICE_ERROR_INIT,
    BMA_EVENT_SERVICE_ERROR_READ,
    BMA_EVENT_SERVICE_DISABLED
} bma_event_service_status_t;

typedef struct
{
    /* Accepted Any-Motion event after service-level cooldown policy. */
    bool any_motion;

    /* Raw Bosch Any-Motion status before cooldown suppression. */
    bool raw_any_motion;
    bool suppressed_by_cooldown;
    uint16_t raw_interrupt_status;
} bma_event_service_sample_t;

/* Phase-5 Live Expressions: resolved policy and cooldown behavior. */
extern bolus_bma_event_sensitivity_t bma_event_service_diag_profile;
extern bool bma_event_service_diag_interrupt_enabled;
extern uint16_t bma_event_service_diag_threshold_mg;
extern uint16_t bma_event_service_diag_duration_ms;
extern uint16_t bma_event_service_diag_cooldown_s;
extern uint32_t bma_event_service_diag_accepted_count;
extern uint32_t bma_event_service_diag_suppressed_count;

/*
 * Must be called after SensorService_InitBma(), because the latter uploads the
 * Bosch BMA456H feature image and establishes the continuous sentinel path.
 */
bma_event_service_status_t BmaEventService_Init(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config);

/*
 * Read/acknowledge BMA feature status in thread/main context, never in ISR.
 * The raw status is always acknowledged; cooldown only controls whether the
 * raw Any-Motion indication is promoted to an accepted service event.
 */
bma_event_service_status_t BmaEventService_Read(
    bma_event_service_sample_t *sample);

bool BmaEventService_IsReady(void);

#endif /* BMA_EVENT_SERVICE_H */
