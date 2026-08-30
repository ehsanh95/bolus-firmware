#include "bma_event_service.h"

#include "bma456_event.h"
#include "bma_irq_diag.h"
#include "fault_manager.h"

static bool s_event_service_ready = false;
static uint32_t s_event_cooldown_ms = 0U;
static uint32_t s_last_accepted_tick_ms = 0U;
static bool s_has_accepted_event = false;

bolus_bma_event_sensitivity_t bma_event_service_diag_profile =
    BOLUS_BMA_EVENT_SENSITIVITY_RAW;
bool bma_event_service_diag_interrupt_enabled = false;
uint16_t bma_event_service_diag_threshold_mg = 0U;
uint16_t bma_event_service_diag_duration_ms = 0U;
uint16_t bma_event_service_diag_cooldown_s = 0U;
uint32_t bma_event_service_diag_accepted_count = 0U;
uint32_t bma_event_service_diag_suppressed_count = 0U;

bma_event_service_status_t BmaEventService_Init(
    SPI_HandleTypeDef *hspi,
    const bolus_runtime_config_t *config)
{
    bma456_event_config_t event_config = {0};
    bolus_bma_event_settings_t event_settings = {0};
    bma456_event_status_t status;

    if ((hspi == NULL) || (config == NULL))
    {
        return BMA_EVENT_SERVICE_ERROR_PARAM;
    }

    s_event_service_ready = false;
    s_event_cooldown_ms = 0U;
    s_last_accepted_tick_ms = 0U;
    s_has_accepted_event = false;

    bma_event_service_diag_profile =
        BOLUS_BMA_EVENT_SENSITIVITY_RAW;
    bma_event_service_diag_interrupt_enabled = false;
    bma_event_service_diag_threshold_mg = 0U;
    bma_event_service_diag_duration_ms = 0U;
    bma_event_service_diag_cooldown_s = 0U;
    bma_event_service_diag_accepted_count = 0U;
    bma_event_service_diag_suppressed_count = 0U;

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return BMA_EVENT_SERVICE_ERROR_CONFIG;
    }

    if ((!config->event_processing.enable) ||
        (!config->bma.motion_interrupt_enable))
    {
        BmaIrqDiag_Disable();
        return BMA_EVENT_SERVICE_DISABLED;
    }

    if (!BolusRuntimeConfig_ResolveBmaEventSettings(config, &event_settings))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return BMA_EVENT_SERVICE_ERROR_CONFIG;
    }

    event_config.enable = event_settings.interrupt_enable;
    event_config.threshold_mg = event_settings.threshold_mg;
    event_config.duration_ms = event_settings.duration_ms;

    status = BMA456Event_Init(hspi, &event_config);
    if (status != BMA456_EVENT_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return BMA_EVENT_SERVICE_ERROR_INIT;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_CONFIG_INVALID);

    bma_event_service_diag_profile =
        config->event_processing.bma_event_sensitivity_level;
    bma_event_service_diag_interrupt_enabled = event_settings.interrupt_enable;
    bma_event_service_diag_threshold_mg = event_settings.threshold_mg;
    bma_event_service_diag_duration_ms = event_settings.duration_ms;
    bma_event_service_diag_cooldown_s = event_settings.cooldown_s;

    if (!event_settings.interrupt_enable)
    {
        /*
         * OFF means no Any-Motion wake/event path. BMA itself remains owned by
         * SensorService so scheduled acquisition can still read it later.
         */
        BmaIrqDiag_Disable();
        return BMA_EVENT_SERVICE_DISABLED;
    }

    s_event_cooldown_ms = (uint32_t)event_settings.cooldown_s * 1000UL;
    s_event_service_ready = BMA456Event_IsReady();

    /*
     * Isolated PC7/EXTI7 harness: ISR only clears/counts the edge. No BMA
     * status read, TMP/MPU acquisition, or event processing occurs in ISR.
     */
    if (s_event_service_ready)
    {
        BmaIrqDiag_EnableCounterOnly();
    }

    return s_event_service_ready ?
        BMA_EVENT_SERVICE_OK : BMA_EVENT_SERVICE_ERROR_INIT;
}

bma_event_service_status_t BmaEventService_Read(
    bma_event_service_sample_t *sample)
{
    bma456_event_status_t status;
    bool raw_any_motion = false;
    uint32_t now_ms;

    if (sample == NULL)
    {
        return BMA_EVENT_SERVICE_ERROR_PARAM;
    }

    sample->any_motion = false;
    sample->raw_any_motion = false;
    sample->suppressed_by_cooldown = false;
    sample->raw_interrupt_status = 0U;

    if (!s_event_service_ready)
    {
        return BMA_EVENT_SERVICE_ERROR_READ;
    }

    status = BMA456Event_ReadStatus(
        &raw_any_motion,
        &sample->raw_interrupt_status);

    if (status != BMA456_EVENT_OK)
    {
        FaultManager_Raise(BOLUS_FAULT_BMA456_COMM);
        return BMA_EVENT_SERVICE_ERROR_READ;
    }

    (void)FaultManager_ClearFault(BOLUS_FAULT_BMA456_COMM);

    sample->raw_any_motion = raw_any_motion;

    if (!raw_any_motion)
    {
        return BMA_EVENT_SERVICE_OK;
    }

    /*
     * Cooldown is deliberately applied after reading/acknowledging the Bosch
     * feature status. Therefore INT1 remains correctly re-armed even when an
     * event is suppressed. The ISR remains counter-only.
     */
    now_ms = HAL_GetTick();

    if ((s_event_cooldown_ms == 0U) ||
        (!s_has_accepted_event) ||
        ((uint32_t)(now_ms - s_last_accepted_tick_ms) >= s_event_cooldown_ms))
    {
        sample->any_motion = true;
        s_last_accepted_tick_ms = now_ms;
        s_has_accepted_event = true;
        bma_event_service_diag_accepted_count++;
    }
    else
    {
        sample->suppressed_by_cooldown = true;
        bma_event_service_diag_suppressed_count++;
    }

    return BMA_EVENT_SERVICE_OK;
}

bool BmaEventService_IsReady(void)
{
    return s_event_service_ready && BMA456Event_IsReady();
}
