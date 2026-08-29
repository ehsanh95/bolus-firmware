#include "event_acquisition_service.h"

#include <string.h>

static bool AllRequestedValid(const event_acquisition_capture_t *capture)
{
    return ((capture->validity & capture->requested_validity) ==
            capture->requested_validity);
}

event_acquisition_status_t EventAcquisitionService_Capture(
    const bolus_runtime_config_t *config,
    uint32_t trigger_timestamp_ms,
    event_acquisition_capture_t *capture)
{
    if ((config == NULL) || (capture == NULL))
    {
        return EVENT_ACQUISITION_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        return EVENT_ACQUISITION_ERROR_CONFIG;
    }

    memset(capture, 0, sizeof(*capture));

    capture->trigger_timestamp_ms = trigger_timestamp_ms;
    capture->requested_validity =
        EVENT_ACQUISITION_VALID_BMA |
        EVENT_ACQUISITION_VALID_TMP;

    if (config->mpu.event_trigger_enable)
    {
        capture->requested_validity |= EVENT_ACQUISITION_VALID_MPU;
    }

    capture->bma_status = SENSOR_SERVICE_ERROR_BMA_READ;
    capture->tmp_status = SENSOR_SERVICE_ERROR_TMP_READ;
    capture->mpu_status = SENSOR_SERVICE_ERROR_MPU_READ;

    if (SensorService_IsBmaReady())
    {
        capture->bma_status = SensorService_ReadBmaSample(&capture->bma);
        if (capture->bma_status == SENSOR_SERVICE_OK)
        {
            capture->validity |= EVENT_ACQUISITION_VALID_BMA;
        }
    }

    if (SensorService_IsTemperatureReady())
    {
        capture->tmp_status =
            SensorService_ReadTemperatureOneShot(&capture->tmp);
        if (capture->tmp_status == SENSOR_SERVICE_OK)
        {
            capture->validity |= EVENT_ACQUISITION_VALID_TMP;
        }
    }

    if (config->mpu.event_trigger_enable && SensorService_IsMpuReady())
    {
        capture->mpu_status = SensorService_ReadMpuSample(&capture->mpu);
        if (capture->mpu_status == SENSOR_SERVICE_OK)
        {
            capture->validity |= EVENT_ACQUISITION_VALID_MPU;
        }
    }

    if (AllRequestedValid(capture))
    {
        return EVENT_ACQUISITION_OK;
    }

    if (capture->validity != 0U)
    {
        return EVENT_ACQUISITION_PARTIAL;
    }

    return EVENT_ACQUISITION_ERROR_NO_DATA;
}
