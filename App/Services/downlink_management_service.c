#include "downlink_management_service.h"

#include <string.h>

/* [UNTESTED] No clean-build, gateway, RX1/RX2 or live-apply validation yet. */
#define DOWNLINK_HEADER_SIZE  4U

static bolus_runtime_config_t *s_runtime_config = NULL;
static bool s_initialized = false;
static bool s_last_accepted_transaction_valid = false;
static uint8_t s_last_accepted_transaction_id = 0U;

downlink_management_diag_t downlink_management_diag = {0};

static uint16_t ReadU16Le(const uint8_t *value)
{
    return (uint16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8));
}

static uint32_t ReadU32Le(const uint8_t *value)
{
    return ((uint32_t)value[0]) |
           ((uint32_t)value[1] << 8) |
           ((uint32_t)value[2] << 16) |
           ((uint32_t)value[3] << 24);
}

static void BuildResponse(
    uint8_t transaction_id,
    downlink_result_t result,
    downlink_apply_mask_t apply_mask,
    uint8_t *response,
    uint8_t *response_size)
{
    uint16_t config_version = 0U;

    if (s_runtime_config != NULL)
    {
        config_version = s_runtime_config->version;
    }

    response[0] = DOWNLINK_MANAGEMENT_RESPONSE_MAGIC;
    response[1] = DOWNLINK_MANAGEMENT_PROTOCOL_VERSION;
    response[2] = transaction_id;
    response[3] = (uint8_t)result;
    response[4] = (uint8_t)(apply_mask & 0xFFU);
    response[5] = (uint8_t)((apply_mask >> 8) & 0xFFU);
    response[6] = (uint8_t)(config_version & 0xFFU);
    response[7] = (uint8_t)((config_version >> 8) & 0xFFU);
    *response_size = DOWNLINK_MANAGEMENT_RESPONSE_SIZE;
}

static downlink_result_t ApplyCommand(
    uint8_t command_id,
    const uint8_t *value,
    uint8_t length,
    bolus_runtime_config_t *candidate,
    downlink_apply_mask_t *apply_mask)
{
    switch ((downlink_command_id_t)command_id)
    {
        case DOWNLINK_CMD_SET_BMA_EVENT_SENSITIVITY:
            if (length != 1U) return DOWNLINK_RESULT_ERROR_LENGTH;
            if (value[0] > (uint8_t)BOLUS_BMA_EVENT_SENSITIVITY_OFF)
                return DOWNLINK_RESULT_ERROR_VALUE;
            candidate->event_processing.bma_event_sensitivity_level =
                (bolus_bma_event_sensitivity_t)value[0];
            if (candidate->event_processing.bma_event_sensitivity_level !=
                BOLUS_BMA_EVENT_SENSITIVITY_RAW)
            {
                candidate->event_processing.bma_event_threshold_mg = 0U;
                candidate->event_processing.bma_event_duration_ms = 0U;
                candidate->event_processing.bma_event_cooldown_s = 0U;
            }
            *apply_mask |= DOWNLINK_APPLY_BMA_EVENT;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_BMA_STEP_SENSITIVITY:
            if (length != 1U) return DOWNLINK_RESULT_ERROR_LENGTH;
            if (value[0] > (uint8_t)BOLUS_BMA_STEP_SENSITIVITY_LEVEL_7)
                return DOWNLINK_RESULT_ERROR_VALUE;
            candidate->bma.step_sensitivity =
                (bolus_bma_step_sensitivity_t)value[0];
            *apply_mask |= DOWNLINK_APPLY_BMA_SENSOR;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_EPISODE_RETRIGGER_GUARD_MS:
            if (length != 2U) return DOWNLINK_RESULT_ERROR_LENGTH;
            candidate->event_processing.episode_retrigger_guard_ms = ReadU16Le(value);
            *apply_mask |= DOWNLINK_APPLY_EVENT_EPISODE;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_EPISODE_QUIET_TIMEOUT_S:
            if (length != 2U) return DOWNLINK_RESULT_ERROR_LENGTH;
            candidate->event_processing.episode_quiet_timeout_s = ReadU16Le(value);
            *apply_mask |= DOWNLINK_APPLY_EVENT_EPISODE;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_TMP_SAMPLE_PERIOD_S:
            if (length != 4U) return DOWNLINK_RESULT_ERROR_LENGTH;
            candidate->temperature.sample_period_s = ReadU32Le(value);
            return DOWNLINK_RESULT_ACCEPTED_NO_LIVE_RECONFIG;

        case DOWNLINK_CMD_SET_MPU_BURST_DURATION_MS:
            if (length != 2U) return DOWNLINK_RESULT_ERROR_LENGTH;
            candidate->mpu.burst_duration_ms = ReadU16Le(value);
            *apply_mask |= DOWNLINK_APPLY_MPU_SENSOR;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_UPLINK_PERIOD_S:
            if (length != 4U) return DOWNLINK_RESULT_ERROR_LENGTH;
            candidate->radio.uplink_period_s = ReadU32Le(value);
            *apply_mask |= DOWNLINK_APPLY_TELEMETRY_WINDOW;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_EVENT_ENABLE:
            if (length != 1U) return DOWNLINK_RESULT_ERROR_LENGTH;
            if (value[0] > 1U) return DOWNLINK_RESULT_ERROR_VALUE;
            candidate->event_processing.enable = (value[0] != 0U);
            *apply_mask |= DOWNLINK_APPLY_BMA_EVENT | DOWNLINK_APPLY_EVENT_EPISODE;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        case DOWNLINK_CMD_SET_MPU_EVENT_TRIGGER_ENABLE:
            if (length != 1U) return DOWNLINK_RESULT_ERROR_LENGTH;
            if (value[0] > 1U) return DOWNLINK_RESULT_ERROR_VALUE;
            candidate->mpu.event_trigger_enable = (value[0] != 0U);
            *apply_mask |= DOWNLINK_APPLY_MPU_SENSOR;
            return DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY;

        default:
            return DOWNLINK_RESULT_ERROR_COMMAND;
    }
}

bool DownlinkManagementService_Init(bolus_runtime_config_t *runtime_config)
{
    memset(&downlink_management_diag, 0, sizeof(downlink_management_diag));
    s_runtime_config = NULL;
    s_initialized = false;
    s_last_accepted_transaction_valid = false;
    s_last_accepted_transaction_id = 0U;
    downlink_management_diag.validation_state = DOWNLINK_VALIDATION_UNTESTED;

    if ((runtime_config == NULL) || !BolusRuntimeConfig_Validate(runtime_config))
    {
        downlink_management_diag.last_result = DOWNLINK_RESULT_ERROR_PARAM;
        return false;
    }

    s_runtime_config = runtime_config;
    s_initialized = true;
    downlink_management_diag.initialized = true;
    downlink_management_diag.last_result = DOWNLINK_RESULT_ACCEPTED_NO_LIVE_RECONFIG;
    return true;
}

downlink_result_t DownlinkManagementService_HandleFrame(
    const uint8_t *payload,
    uint8_t size,
    uint8_t *response,
    uint8_t response_capacity,
    uint8_t *response_size)
{
    bolus_runtime_config_t candidate;
    downlink_apply_mask_t apply_mask = DOWNLINK_APPLY_NONE;
    downlink_result_t result = DOWNLINK_RESULT_ERROR_PARAM;
    uint8_t transaction_id = 0U;
    uint8_t command_count = 0U;
    uint8_t offset = DOWNLINK_HEADER_SIZE;
    uint8_t command_index;

    if (response_size != NULL) *response_size = 0U;

    if ((response == NULL) || (response_size == NULL) ||
        (response_capacity < DOWNLINK_MANAGEMENT_RESPONSE_SIZE))
        return DOWNLINK_RESULT_ERROR_PARAM;

    if (!s_initialized || (s_runtime_config == NULL))
    {
        BuildResponse(0U, DOWNLINK_RESULT_ERROR_NOT_INITIALIZED,
                      DOWNLINK_APPLY_NONE, response, response_size);
        return DOWNLINK_RESULT_ERROR_NOT_INITIALIZED;
    }

    downlink_management_diag.rx_frame_count++;

    if (payload == NULL) goto reject;
    if (size >= 3U) transaction_id = payload[2];
    if (size < DOWNLINK_HEADER_SIZE)
    {
        result = DOWNLINK_RESULT_ERROR_LENGTH;
        goto reject;
    }
    if (payload[0] != DOWNLINK_MANAGEMENT_REQUEST_MAGIC)
    {
        result = DOWNLINK_RESULT_ERROR_MAGIC;
        goto reject;
    }
    if (payload[1] != DOWNLINK_MANAGEMENT_PROTOCOL_VERSION)
    {
        result = DOWNLINK_RESULT_ERROR_VERSION;
        goto reject;
    }

    command_count = payload[3];

    if (s_last_accepted_transaction_valid &&
        (transaction_id == s_last_accepted_transaction_id))
    {
        downlink_management_diag.duplicate_count++;
        downlink_management_diag.last_transaction_id = transaction_id;
        downlink_management_diag.last_transaction_valid = true;
        downlink_management_diag.last_command_count = command_count;
        downlink_management_diag.last_apply_mask = DOWNLINK_APPLY_NONE;
        downlink_management_diag.last_result = DOWNLINK_RESULT_DUPLICATE_TRANSACTION;
        BuildResponse(transaction_id, DOWNLINK_RESULT_DUPLICATE_TRANSACTION,
                      DOWNLINK_APPLY_NONE, response, response_size);
        return DOWNLINK_RESULT_DUPLICATE_TRANSACTION;
    }

    candidate = *s_runtime_config;

    for (command_index = 0U; command_index < command_count; command_index++)
    {
        uint8_t command_id;
        uint8_t length;

        if ((uint16_t)offset + 2U > size)
        {
            result = DOWNLINK_RESULT_ERROR_LENGTH;
            goto reject;
        }

        command_id = payload[offset++];
        length = payload[offset++];
        downlink_management_diag.last_command_id = command_id;

        if ((uint16_t)offset + length > size)
        {
            result = DOWNLINK_RESULT_ERROR_LENGTH;
            goto reject;
        }

        result = ApplyCommand(command_id, &payload[offset], length,
                              &candidate, &apply_mask);
        if ((result != DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY) &&
            (result != DOWNLINK_RESULT_ACCEPTED_NO_LIVE_RECONFIG))
            goto reject;

        offset = (uint8_t)(offset + length);
    }

    if (offset != size)
    {
        result = DOWNLINK_RESULT_ERROR_LENGTH;
        goto reject;
    }

    if (!BolusRuntimeConfig_Validate(&candidate))
    {
        result = DOWNLINK_RESULT_ERROR_CONFIG;
        goto reject;
    }

    if (memcmp(&candidate, s_runtime_config, sizeof(candidate)) != 0)
    {
        /* Atomic RAM commit only. Cached service activation is still pending. */
        *s_runtime_config = candidate;
        downlink_management_diag.ram_config_commit_count++;
    }

    downlink_management_diag.pending_apply_mask |= apply_mask;
    downlink_management_diag.accepted_count++;
    downlink_management_diag.last_transaction_id = transaction_id;
    downlink_management_diag.last_transaction_valid = true;
    downlink_management_diag.last_command_count = command_count;
    downlink_management_diag.last_apply_mask = apply_mask;

    s_last_accepted_transaction_valid = true;
    s_last_accepted_transaction_id = transaction_id;

    result = (apply_mask != DOWNLINK_APPLY_NONE) ?
        DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY :
        DOWNLINK_RESULT_ACCEPTED_NO_LIVE_RECONFIG;
    downlink_management_diag.last_result = result;
    BuildResponse(transaction_id, result, apply_mask, response, response_size);
    return result;

reject:
    downlink_management_diag.rejected_count++;
    downlink_management_diag.last_transaction_id = transaction_id;
    downlink_management_diag.last_transaction_valid = (size >= 3U);
    downlink_management_diag.last_command_count = command_count;
    downlink_management_diag.last_apply_mask = DOWNLINK_APPLY_NONE;
    downlink_management_diag.last_result = result;

    /* A rejected frame is a protocol/input error, not a local device fault. */
    BuildResponse(transaction_id, result, DOWNLINK_APPLY_NONE,
                  response, response_size);
    return result;
}

downlink_apply_mask_t DownlinkManagementService_GetPendingApplyMask(void)
{
    return downlink_management_diag.pending_apply_mask;
}

void DownlinkManagementService_MarkApplyResult(
    downlink_apply_mask_t attempted_mask,
    bool success)
{
    attempted_mask &= downlink_management_diag.pending_apply_mask;
    if (attempted_mask == DOWNLINK_APPLY_NONE) return;

    if (success)
    {
        downlink_management_diag.pending_apply_mask &=
            (downlink_apply_mask_t)(~attempted_mask);
        downlink_management_diag.apply_complete_count++;
    }
    else
    {
        downlink_management_diag.apply_failure_count++;
    }
}

bool DownlinkManagementService_IsInitialized(void)
{
    return s_initialized;
}
