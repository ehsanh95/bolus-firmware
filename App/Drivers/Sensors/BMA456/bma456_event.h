#ifndef BMA456_EVENT_H
#define BMA456_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32l4xx_hal.h"

/*
 * Bolus-specific BMA456H Any-Motion wrapper.
 *
 * This path is intentionally independent from the native Step Counter
 * sensitivity. The event detector is the low-power sentinel that will later
 * wake the STM32 and trigger TMP117 + MPU6050 acquisition.
 */
typedef enum
{
    BMA456_EVENT_OK = 0,
    BMA456_EVENT_ERROR_PARAM,
    BMA456_EVENT_ERROR_CONFIG,
    BMA456_EVENT_ERROR_COMM,
    BMA456_EVENT_ERROR_NOT_READY
} bma456_event_status_t;

typedef struct
{
    bool enable;

    /*
     * 0 = preserve the Bosch feature-image value and read it back.
     * Non-zero threshold is expressed in mg and converted to Bosch 5.11 g.
     */
    uint16_t threshold_mg;

    /*
     * 0 = preserve the Bosch feature-image value.
     * Non-zero values must be exact 20-ms multiples because Bosch expresses
     * Any-Motion duration in 50-Hz samples.
     */
    uint16_t duration_ms;
} bma456_event_config_t;

/*
 * Configure BMA456H Any-Motion on all axes and map it to sensor INT1.
 * The BMA power rail and feature configuration must already be active.
 */
bma456_event_status_t BMA456Event_Init(
    SPI_HandleTypeDef *hspi,
    const bma456_event_config_t *config);

/*
 * Read/acknowledge BMA feature interrupt status outside the MCU ISR.
 * any_motion is true only when the Bosch ANY_MOT status bit is present.
 */
bma456_event_status_t BMA456Event_ReadStatus(
    bool *any_motion,
    uint16_t *raw_interrupt_status);

bool BMA456Event_IsReady(void);

#endif /* BMA456_EVENT_H */
