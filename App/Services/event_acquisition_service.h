#ifndef EVENT_ACQUISITION_SERVICE_H
#define EVENT_ACQUISITION_SERVICE_H

#include <stdint.h>

#include "bolus_runtime_config.h"
#include "sensor_service.h"

typedef uint8_t event_acquisition_validity_t;

#define EVENT_ACQUISITION_VALID_BMA  (1U << 0)
#define EVENT_ACQUISITION_VALID_TMP  (1U << 1)
#define EVENT_ACQUISITION_VALID_MPU  (1U << 2)

typedef enum
{
    EVENT_ACQUISITION_OK = 0,
    EVENT_ACQUISITION_PARTIAL,
    EVENT_ACQUISITION_ERROR_PARAM,
    EVENT_ACQUISITION_ERROR_CONFIG,
    EVENT_ACQUISITION_ERROR_NO_DATA
} event_acquisition_status_t;

/*
 * One confirmed BMA-triggered multisensor snapshot.
 *
 * This is intentionally a physical acquisition record, not a behavior label.
 * The EventProcessor/EventAggregator layer decides what the measurements mean.
 *
 * V1 uses the already bench-proven single MPU sample. A later commit replaces
 * the MPU snapshot with a bounded burst summary without changing interrupt
 * ownership: ISR -> pending flag -> main/thread context -> acquisition service.
 */
typedef struct
{
    uint32_t trigger_timestamp_ms;

    event_acquisition_validity_t requested_validity;
    event_acquisition_validity_t validity;

    sensor_service_status_t bma_status;
    sensor_service_status_t tmp_status;
    sensor_service_status_t mpu_status;

    sensor_service_bma_sample_t bma;
    sensor_service_temperature_sample_t tmp;
    sensor_service_mpu_sample_t mpu;
} event_acquisition_capture_t;

/*
 * Capture BMA + TMP117 and, when RuntimeConfig enables event triggering,
 * MPU6050. Individual sensor failure does not discard data from healthy paths.
 */
event_acquisition_status_t EventAcquisitionService_Capture(
    const bolus_runtime_config_t *config,
    uint32_t trigger_timestamp_ms,
    event_acquisition_capture_t *capture);

#endif /* EVENT_ACQUISITION_SERVICE_H */
