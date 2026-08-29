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
    bool any_motion;
    uint16_t raw_interrupt_status;
} bma_event_service_sample_t;

/*
 * Must be called after SensorService_InitBma(), because the latter uploads the
 * Bosch BMA456H feature image and establishes the continuous sentinel path.
 */
bma_event_service_status_t BmaEventService_Init(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config);

/* Read/acknowledge BMA feature status in thread/main context, never in ISR. */
bma_event_service_status_t BmaEventService_Read(
    bma_event_service_sample_t *sample);

bool BmaEventService_IsReady(void);

#endif /* BMA_EVENT_SERVICE_H */
