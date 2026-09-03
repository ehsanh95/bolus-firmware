#ifndef DOWNLINK_MANAGEMENT_SERVICE_H
#define DOWNLINK_MANAGEMENT_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../Config/bolus_runtime_config.h"

/*
 * ============================================================
 * [UNTESTED] LoRaWAN Downlink Management staging contract
 * ============================================================
 *
 * IMPORTANT:
 * - This code has NOT been validated by a clean CubeIDE build yet.
 * - This code has NOT been validated on hardware/gateway/network-server yet.
 * - Accepted commands update the application RuntimeConfig atomically in RAM.
 * - Services that cache configuration still require a later live-reconfigure
 *   step. pending_apply_mask makes that incomplete activation explicit.
 * - RF fields added in this revision are command-decodable but are still
 *   explicitly UNTESTED. Some are legacy/raw-radio policy fields and must not
 *   be described as active LoRaWAN PHY settings until the MAC apply path is
 *   implemented and validated.
 * - No production claim is allowed until the apply path is implemented and
 *   hardware/network tests are recorded.
 */

#define DOWNLINK_MANAGEMENT_PROTOCOL_VERSION       1U
#define DOWNLINK_MANAGEMENT_REQUEST_MAGIC          0xD1U
#define DOWNLINK_MANAGEMENT_RESPONSE_MAGIC         0xD2U
#define DOWNLINK_MANAGEMENT_RESPONSE_SIZE          8U

typedef uint16_t downlink_apply_mask_t;

#define DOWNLINK_APPLY_NONE                 ((downlink_apply_mask_t)0U)
#define DOWNLINK_APPLY_BMA_EVENT             ((downlink_apply_mask_t)(1U << 0))
#define DOWNLINK_APPLY_BMA_SENSOR            ((downlink_apply_mask_t)(1U << 1))
#define DOWNLINK_APPLY_EVENT_EPISODE         ((downlink_apply_mask_t)(1U << 2))
#define DOWNLINK_APPLY_MPU_SENSOR            ((downlink_apply_mask_t)(1U << 3))
#define DOWNLINK_APPLY_TELEMETRY_WINDOW      ((downlink_apply_mask_t)(1U << 4))
#define DOWNLINK_APPLY_RADIO_POLICY          ((downlink_apply_mask_t)(1U << 5))

/*
 * Request format (little-endian values):
 *
 * byte 0  : 0xD1 request magic
 * byte 1  : protocol version (=1)
 * byte 2  : transaction id
 * byte 3  : TLV command count
 * byte 4+ : repeated [command_id][length][value bytes...]
 *
 * The LoRaWAN MIC already provides link-layer integrity/authentication, so no
 * extra application CRC is added in this staging protocol.
 */
typedef enum
{
    DOWNLINK_CMD_SET_BMA_EVENT_SENSITIVITY = 0x01,
    DOWNLINK_CMD_SET_BMA_STEP_SENSITIVITY = 0x02,
    DOWNLINK_CMD_SET_EPISODE_RETRIGGER_GUARD_MS = 0x03,
    DOWNLINK_CMD_SET_EPISODE_QUIET_TIMEOUT_S = 0x04,
    DOWNLINK_CMD_SET_TMP_SAMPLE_PERIOD_S = 0x05,
    DOWNLINK_CMD_SET_MPU_BURST_DURATION_MS = 0x06,
    DOWNLINK_CMD_SET_UPLINK_PERIOD_S = 0x07,
    DOWNLINK_CMD_SET_EVENT_ENABLE = 0x08,
    DOWNLINK_CMD_SET_MPU_EVENT_TRIGGER_ENABLE = 0x09,

    /* [UNTESTED] Runtime radio/RF policy commands. */
    DOWNLINK_CMD_SET_RF_TX_POWER_DBM = 0x0A,
    DOWNLINK_CMD_SET_RF_SPREADING_FACTOR = 0x0B,
    DOWNLINK_CMD_SET_RF_BANDWIDTH_INDEX = 0x0C,
    DOWNLINK_CMD_SET_RF_CODING_RATE = 0x0D,
    DOWNLINK_CMD_SET_RF_TX_TIMEOUT_MS = 0x0E,
    DOWNLINK_CMD_SET_RF_RETRY_DELAY_MS = 0x0F,
    DOWNLINK_CMD_SET_RF_MAX_TX_ATTEMPTS = 0x10
} downlink_command_id_t;

typedef enum
{
    DOWNLINK_RESULT_ACCEPTED_PENDING_APPLY = 0x00,
    DOWNLINK_RESULT_ACCEPTED_NO_LIVE_RECONFIG = 0x01,
    DOWNLINK_RESULT_DUPLICATE_TRANSACTION = 0x02,

    DOWNLINK_RESULT_ERROR_PARAM = 0x80,
    DOWNLINK_RESULT_ERROR_MAGIC = 0x81,
    DOWNLINK_RESULT_ERROR_VERSION = 0x82,
    DOWNLINK_RESULT_ERROR_LENGTH = 0x83,
    DOWNLINK_RESULT_ERROR_COMMAND = 0x84,
    DOWNLINK_RESULT_ERROR_VALUE = 0x85,
    DOWNLINK_RESULT_ERROR_CONFIG = 0x86,
    DOWNLINK_RESULT_ERROR_NOT_INITIALIZED = 0x87
} downlink_result_t;

typedef enum
{
    DOWNLINK_VALIDATION_UNTESTED = 0,
    DOWNLINK_VALIDATION_BUILD_ONLY,
    DOWNLINK_VALIDATION_HARDWARE_PASS
} downlink_validation_state_t;

typedef struct
{
    bool initialized;
    downlink_validation_state_t validation_state;

    uint32_t rx_frame_count;
    uint32_t accepted_count;
    uint32_t duplicate_count;
    uint32_t rejected_count;

    uint8_t last_transaction_id;
    bool last_transaction_valid;
    uint8_t last_command_count;
    uint8_t last_command_id;
    downlink_result_t last_result;

    downlink_apply_mask_t last_apply_mask;
    downlink_apply_mask_t pending_apply_mask;

    uint32_t ram_config_commit_count;
    uint32_t apply_complete_count;
    uint32_t apply_failure_count;
} downlink_management_diag_t;

extern downlink_management_diag_t downlink_management_diag;

/* RuntimeConfig pointer must remain valid for the life of the application. */
bool DownlinkManagementService_Init(bolus_runtime_config_t *runtime_config);

/*
 * Decode one application downlink and build an 8-byte ACK/NACK response.
 * The caller is responsible for checking the configured LoRaWAN FPort before
 * calling this function.
 */
downlink_result_t DownlinkManagementService_HandleFrame(
    const uint8_t *payload,
    uint8_t size,
    uint8_t *response,
    uint8_t response_capacity,
    uint8_t *response_size);

/*
 * Live hardware/service reconfiguration is deliberately separate from RAM
 * config acceptance. Until these bits are cleared by the apply layer, the
 * configuration must be treated as only partially activated.
 */
downlink_apply_mask_t DownlinkManagementService_GetPendingApplyMask(void);

void DownlinkManagementService_MarkApplyResult(
    downlink_apply_mask_t attempted_mask,
    bool success);

bool DownlinkManagementService_IsInitialized(void);

#endif /* DOWNLINK_MANAGEMENT_SERVICE_H */
