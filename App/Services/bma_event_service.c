#include "bma_event_service.h"

#include "bma456_event.h"
#include "fault_manager.h"

static bool s_event_service_ready = false;

bma_event_service_status_t BmaEventService_Init(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config)
{
    bma456_event_config_t event_config = {0};
    bma456_event_status_t status;

    if ((hspi == NULL) || (config == NULL))
    {
        return BMA_EVENT_SERVICE_ERROR_PARAM;
    }

    s_event_service_ready = false;

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return BMA_EVENT_SERVICE_ERROR_CONFIG;
    }

    if ((!config->event_processing.enable) ||
        (!config->bma.motion_interrupt_enable))
    {
        return BMA_EVENT_SERVICE_DISABLED;
    }

    event_config.enable = true;
    event_config.threshold_mg =
        config->event_processing.bma_event_threshold_mg;
    event_config.duration_ms =
        config->event_processing.bma_event_duration_ms;

    status = BMA456Event_Init(hspi, &event_config);
    if (status != BMA456_EVENT_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return BMA_EVENT_SERVICE_ERROR_INIT;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID);

    s_event_service_ready = BMA456Event_IsReady();

    return s_event_service_ready ?
        BMA_EVENT_SERVICE_OK : BMA_EVENT_SERVICE_ERROR_INIT;
}

bma_event_service_status_t BmaEventService_Read(
    bma_event_service_sample_t *sample)
{
    bma456_event_status_t status;

    if (sample == NULL)
    {
        return BMA_EVENT_SERVICE_ERROR_PARAM;
    }

    sample->any_motion = false;
    sample->raw_interrupt_status = 0U;

    if (!s_event_service_ready)
    {
        return BMA_EVENT_SERVICE_ERROR_READ;
    }

    status = BMA456Event_ReadStatus(
        &sample->any_motion,
        &sample->raw_interrupt_status);

    if (status != BMA456_EVENT_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return BMA_EVENT_SERVICE_ERROR_READ;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);

    return BMA_EVENT_SERVICE_OK;
}

bool BmaEventService_IsReady(void)
{
    return s_event_service_ready && BMA456Event_IsReady();
}
