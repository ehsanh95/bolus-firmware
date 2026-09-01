#include "lorawan_uplink_service.h"

#include "bolus_lorawan_credentials.h"
#include "fault_manager.h"
#include "main.h"
#include "rfm95w_board.h"

#include <stddef.h>
#include <string.h>

#define LORAWAN_UPLINK_BUSY_RETRY_MS  100UL

typedef struct
{
    bool valid;
    uint8_t size;
    uint8_t attempts;
    uint16_t sequence;
    uint8_t payload[BOLUS_LORAWAN_MAX_APP_PAYLOAD];
} lorawan_uplink_queue_entry_t;

static lorawan_uplink_queue_entry_t s_queue[BOLUS_LORAWAN_QUEUE_DEPTH];
static uint8_t s_queue_head = 0U;
static uint8_t s_queue_tail = 0U;
static uint8_t s_queue_count = 0U;

static bool s_initialized = false;
static bool s_joined = false;
static bool s_join_in_flight = false;
static bool s_tx_in_flight = false;
static uint32_t s_next_action_tick_ms = 0U;
static uint32_t s_retry_delay_ms = 2000U;
static uint8_t s_max_tx_attempts = 3U;

static uint8_t s_dev_eui[8] = BOLUS_LORAWAN_DEV_EUI_BYTES;
static uint8_t s_join_eui[8] = BOLUS_LORAWAN_JOIN_EUI_BYTES;
static uint8_t s_root_key[16] = BOLUS_LORAWAN_ROOT_KEY_BYTES;

lorawan_uplink_diag_t lorawan_uplink_service_diag = {0};

static bool TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0);
}

static lorawan_uplink_queue_entry_t *QueueHead(void)
{
    if (s_queue_count == 0U)
    {
        return NULL;
    }

    return &s_queue[s_queue_head];
}

static void QueuePop(void)
{
    if (s_queue_count == 0U)
    {
        return;
    }

    memset(&s_queue[s_queue_head], 0, sizeof(s_queue[s_queue_head]));
    s_queue_head = (uint8_t)((s_queue_head + 1U) % BOLUS_LORAWAN_QUEUE_DEPTH);
    s_queue_count--;
    lorawan_uplink_service_diag.queue_count = s_queue_count;
}

static uint8_t MacGetBatteryLevel(void)
{
    /* Battery DevStatus mapping is intentionally deferred to the downlink/power stage. */
    return BAT_LEVEL_NO_MEASURE;
}

static uint16_t MacGetTemperatureLevel(void)
{
    /* Class B is disabled; no temperature callback is required by current policy. */
    return 0U;
}

static void MacGetUniqueId(uint8_t *id)
{
    uint32_t uid0;
    uint32_t uid1;
    uint32_t uid2;

    if (id == NULL)
    {
        return;
    }

    uid0 = HAL_GetUIDw0();
    uid1 = HAL_GetUIDw1();
    uid2 = HAL_GetUIDw2();

    /* Stable 64-bit value derived from the 96-bit STM32 unique ID. */
    id[0] = (uint8_t)(uid2 >> 24);
    id[1] = (uint8_t)(uid2 >> 16);
    id[2] = (uint8_t)(uid2 >> 8);
    id[3] = (uint8_t)uid2;
    uid0 ^= uid1;
    id[4] = (uint8_t)(uid0 >> 24);
    id[5] = (uint8_t)(uid0 >> 16);
    id[6] = (uint8_t)(uid0 >> 8);
    id[7] = (uint8_t)uid0;
}

static void MacNvmDataChange(uint16_t notify_flags)
{
    (void)notify_flags;
    lorawan_uplink_service_diag.nvm_change_count++;

    /* Persistent LoRaMAC context storage is a later integration milestone. */
}

static void MacProcessNotify(void)
{
    lorawan_uplink_service_diag.mac_process_pending = true;
}

static void MacMcpsConfirm(McpsConfirm_t *confirm)
{
    lorawan_uplink_queue_entry_t *entry;

    if (confirm == NULL)
    {
        return;
    }

    s_tx_in_flight = false;
    lorawan_uplink_service_diag.tx_in_flight = false;
    lorawan_uplink_service_diag.last_mcps_confirm_status = confirm->Status;
    lorawan_uplink_service_diag.last_uplink_counter = confirm->UpLinkCounter;
    lorawan_uplink_service_diag.last_tx_air_time_ms = confirm->TxTimeOnAir;

    entry = QueueHead();
    if (entry == NULL)
    {
        lorawan_uplink_service_diag.state =
            s_joined ? LORAWAN_UPLINK_STATE_JOINED_IDLE : LORAWAN_UPLINK_STATE_JOIN_WAIT;
        return;
    }

    if (confirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
    {
        lorawan_uplink_service_diag.tx_success_count++;
        lorawan_uplink_service_diag.last_sequence_completed = entry->sequence;
        QueuePop();
        (void)FaultManager_ClearFault(BOLUS_FAULT_RF_TX_TIMEOUT);
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINED_IDLE;
        s_next_action_tick_ms = HAL_GetTick();
    }
    else
    {
        lorawan_uplink_service_diag.tx_failure_count++;

        if (entry->attempts < s_max_tx_attempts)
        {
            lorawan_uplink_service_diag.tx_retry_count++;
            s_next_action_tick_ms = HAL_GetTick() + s_retry_delay_ms;
            lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
            lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_RETRY_WAIT;
        }
        else
        {
            lorawan_uplink_service_diag.tx_drop_count++;
            FaultManager_Raise(BOLUS_FAULT_RF_TX_TIMEOUT);
            QueuePop();
            s_next_action_tick_ms = HAL_GetTick();
            lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINED_IDLE;
        }
    }
}

static void MacMcpsIndication(McpsIndication_t *indication, LoRaMacRxStatus_t *rx_status)
{
    if (indication == NULL)
    {
        return;
    }

    if ((indication->Status == LORAMAC_EVENT_INFO_STATUS_OK) && indication->RxData)
    {
        lorawan_uplink_service_diag.downlink_count++;
        lorawan_uplink_service_diag.last_downlink_counter = indication->DownLinkCounter;
        lorawan_uplink_service_diag.last_downlink_port = indication->Port;
        lorawan_uplink_service_diag.last_downlink_size = indication->BufferSize;

        if (rx_status != NULL)
        {
            lorawan_uplink_service_diag.last_downlink_rssi_dbm = rx_status->Rssi;
            lorawan_uplink_service_diag.last_downlink_snr_db = rx_status->Snr;
            lorawan_uplink_service_diag.last_downlink_slot = rx_status->RxSlot;
        }
    }

    /* Payload decode/apply is intentionally left for Downlink Management. */
}

static void MacMlmeConfirm(MlmeConfirm_t *confirm)
{
    if ((confirm == NULL) || (confirm->MlmeRequest != MLME_JOIN))
    {
        return;
    }

    s_join_in_flight = false;
    lorawan_uplink_service_diag.join_in_flight = false;
    lorawan_uplink_service_diag.last_join_confirm_status = confirm->Status;

    if (confirm->Status == LORAMAC_EVENT_INFO_STATUS_OK)
    {
        s_joined = true;
        lorawan_uplink_service_diag.joined = true;
        lorawan_uplink_service_diag.join_success_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINED_IDLE;
        s_next_action_tick_ms = HAL_GetTick();
        (void)FaultManager_ClearFault(BOLUS_FAULT_RF_COMM);
    }
    else
    {
        s_joined = false;
        lorawan_uplink_service_diag.joined = false;
        lorawan_uplink_service_diag.join_failure_count++;
        s_next_action_tick_ms = HAL_GetTick() + BOLUS_LORAWAN_JOIN_RETRY_MS;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
}

static void MacMlmeIndication(MlmeIndication_t *indication, LoRaMacRxStatus_t *rx_status)
{
    (void)indication;
    (void)rx_status;
}

static LoRaMacStatus_t SetMib(Mib_t type, MibParam_t param)
{
    MibRequestConfirm_t request;

    memset(&request, 0, sizeof(request));
    request.Type = type;
    request.Param = param;
    return LoRaMacMibSetRequestConfirm(&request);
}

static bool ConfigureMacPolicyAndCredentials(void)
{
    MibParam_t param;
    LoRaMacStatus_t status;

    memset(&param, 0, sizeof(param));
    param.Class = CLASS_A;
    status = SetMib(MIB_DEVICE_CLASS, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

    memset(&param, 0, sizeof(param));
    param.AdrEnable = (BOLUS_LORAWAN_ADR_ENABLE != 0);
    status = SetMib(MIB_ADR, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

    memset(&param, 0, sizeof(param));
    param.EnablePublicNetwork = true;
    status = SetMib(MIB_PUBLIC_NETWORK, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

#if (BOLUS_LORAWAN_CREDENTIALS_PROVISIONED != 0)
    memset(&param, 0, sizeof(param));
    param.DevEui = s_dev_eui;
    status = SetMib(MIB_DEV_EUI, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

    memset(&param, 0, sizeof(param));
    param.JoinEui = s_join_eui;
    status = SetMib(MIB_JOIN_EUI, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

    memset(&param, 0, sizeof(param));
    param.NwkKey = s_root_key;
    status = SetMib(MIB_NWK_KEY, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }

    memset(&param, 0, sizeof(param));
    param.AppKey = s_root_key;
    status = SetMib(MIB_APP_KEY, param);
    if (status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.last_mac_status = status;
        return false;
    }
#endif

    return true;
}

static void TryJoin(uint32_t now_ms)
{
    MlmeReq_t request;
    LoRaMacStatus_t status;

    if (s_joined || s_join_in_flight)
    {
        return;
    }

    memset(&request, 0, sizeof(request));
    request.Type = MLME_JOIN;
    request.Req.Join.Datarate = (uint8_t)BOLUS_LORAWAN_JOIN_DATARATE;

    status = LoRaMacMlmeRequest(&request);
    lorawan_uplink_service_diag.last_mac_status = status;

    if (status == LORAMAC_STATUS_OK)
    {
        s_join_in_flight = true;
        lorawan_uplink_service_diag.join_in_flight = true;
        lorawan_uplink_service_diag.join_request_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINING;
    }
    else if (status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED)
    {
        s_next_action_tick_ms = now_ms + request.ReqReturn.DutyCycleWaitTime;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.duty_cycle_defer_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
    else if (status == LORAMAC_STATUS_BUSY)
    {
        s_next_action_tick_ms = now_ms + LORAWAN_UPLINK_BUSY_RETRY_MS;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.mac_busy_defer_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
    else
    {
        s_next_action_tick_ms = now_ms + BOLUS_LORAWAN_JOIN_RETRY_MS;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.join_failure_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
}

static void TrySendHead(uint32_t now_ms)
{
    lorawan_uplink_queue_entry_t *entry = QueueHead();
    McpsReq_t request;
    LoRaMacStatus_t status;

    if ((entry == NULL) || s_tx_in_flight || !s_joined)
    {
        return;
    }

    memset(&request, 0, sizeof(request));

#if (BOLUS_LORAWAN_CONFIRMED_UPLINK != 0)
    request.Type = MCPS_CONFIRMED;
    request.Req.Confirmed.fPort = BOLUS_LORAWAN_APP_PORT;
    request.Req.Confirmed.fBuffer = entry->payload;
    request.Req.Confirmed.fBufferSize = entry->size;
    request.Req.Confirmed.Datarate = DR_0;
    request.Req.Confirmed.NbTrials = BOLUS_LORAWAN_CONFIRMED_TRIALS;
#else
    request.Type = MCPS_UNCONFIRMED;
    request.Req.Unconfirmed.fPort = BOLUS_LORAWAN_APP_PORT;
    request.Req.Unconfirmed.fBuffer = entry->payload;
    request.Req.Unconfirmed.fBufferSize = entry->size;
    request.Req.Unconfirmed.Datarate = DR_0;
#endif

    status = LoRaMacMcpsRequest(&request, true);
    lorawan_uplink_service_diag.last_mac_status = status;

    if (status == LORAMAC_STATUS_OK)
    {
        entry->attempts++;
        s_tx_in_flight = true;
        lorawan_uplink_service_diag.tx_in_flight = true;
        lorawan_uplink_service_diag.tx_request_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_TX_IN_FLIGHT;
    }
    else if (status == LORAMAC_STATUS_DUTYCYCLE_RESTRICTED)
    {
        s_next_action_tick_ms = now_ms + request.ReqReturn.DutyCycleWaitTime;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.duty_cycle_defer_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_RETRY_WAIT;
    }
    else if (status == LORAMAC_STATUS_BUSY)
    {
        s_next_action_tick_ms = now_ms + LORAWAN_UPLINK_BUSY_RETRY_MS;
        lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
        lorawan_uplink_service_diag.mac_busy_defer_count++;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_RETRY_WAIT;
    }
    else if (status == LORAMAC_STATUS_NO_NETWORK_JOINED)
    {
        s_joined = false;
        lorawan_uplink_service_diag.joined = false;
        s_next_action_tick_ms = now_ms;
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
    else
    {
        lorawan_uplink_service_diag.tx_failure_count++;

        if (entry->attempts < s_max_tx_attempts)
        {
            lorawan_uplink_service_diag.tx_retry_count++;
            s_next_action_tick_ms = now_ms + s_retry_delay_ms;
            lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;
            lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_RETRY_WAIT;
        }
        else
        {
            lorawan_uplink_service_diag.tx_drop_count++;
            FaultManager_Raise(BOLUS_FAULT_RF_COMM);
            QueuePop();
            s_next_action_tick_ms = now_ms;
            lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINED_IDLE;
        }
    }
}

lorawan_uplink_status_t LoRaWanUplinkService_Init(
    const bolus_runtime_config_t *config)
{
    LoRaMacPrimitives_t primitives;
    LoRaMacCallback_t callbacks;
    LoRaMacStatus_t mac_status;

    if ((config == NULL) || !BolusRuntimeConfig_Validate(config))
    {
        return LORAWAN_UPLINK_ERROR_CONFIG;
    }

    memset(s_queue, 0, sizeof(s_queue));
    memset(&lorawan_uplink_service_diag, 0, sizeof(lorawan_uplink_service_diag));
    s_queue_head = 0U;
    s_queue_tail = 0U;
    s_queue_count = 0U;
    s_joined = false;
    s_join_in_flight = false;
    s_tx_in_flight = false;
    s_next_action_tick_ms = HAL_GetTick();
    s_retry_delay_ms = config->radio.retry_delay_ms;
    s_max_tx_attempts = config->radio.max_tx_attempts;

    lorawan_uplink_service_diag.credentials_provisioned =
        (BOLUS_LORAWAN_CREDENTIALS_PROVISIONED != 0);
    lorawan_uplink_service_diag.queue_capacity = BOLUS_LORAWAN_QUEUE_DEPTH;
    lorawan_uplink_service_diag.app_port = BOLUS_LORAWAN_APP_PORT;

    memset(&primitives, 0, sizeof(primitives));
    primitives.MacMcpsConfirm = MacMcpsConfirm;
    primitives.MacMcpsIndication = MacMcpsIndication;
    primitives.MacMlmeConfirm = MacMlmeConfirm;
    primitives.MacMlmeIndication = MacMlmeIndication;

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.GetBatteryLevel = MacGetBatteryLevel;
    callbacks.GetTemperatureLevel = MacGetTemperatureLevel;
    callbacks.GetUniqueId = MacGetUniqueId;
    callbacks.NvmDataChange = MacNvmDataChange;
    callbacks.MacProcessNotify = MacProcessNotify;

    mac_status = LoRaMacInitialization(
        &primitives,
        &callbacks,
        LORAMAC_REGION_EU868);
    lorawan_uplink_service_diag.last_mac_status = mac_status;

    if (mac_status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_ERROR;
        FaultManager_Raise(BOLUS_FAULT_RF_COMM);
        return LORAWAN_UPLINK_ERROR_MAC_INIT;
    }

    if (!ConfigureMacPolicyAndCredentials())
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_ERROR;
        FaultManager_Raise(BOLUS_FAULT_RF_COMM);
        return LORAWAN_UPLINK_ERROR_MIB;
    }

    mac_status = LoRaMacStart();
    lorawan_uplink_service_diag.last_mac_status = mac_status;
    if (mac_status != LORAMAC_STATUS_OK)
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_ERROR;
        FaultManager_Raise(BOLUS_FAULT_RF_COMM);
        return LORAWAN_UPLINK_ERROR_MAC_START;
    }

    s_initialized = true;
    lorawan_uplink_service_diag.initialized = true;
    lorawan_uplink_service_diag.next_action_tick_ms = s_next_action_tick_ms;

    if (lorawan_uplink_service_diag.credentials_provisioned)
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOIN_WAIT;
    }
    else
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_WAIT_CREDENTIALS;
    }

    return LORAWAN_UPLINK_OK;
}

lorawan_uplink_status_t LoRaWanUplinkService_Submit(
    const uint8_t *payload,
    uint8_t size,
    uint16_t sequence)
{
    lorawan_uplink_queue_entry_t *entry;

    if ((payload == NULL) || (size == 0U) ||
        (size > BOLUS_LORAWAN_MAX_APP_PAYLOAD))
    {
        return LORAWAN_UPLINK_ERROR_PARAM;
    }

    if (!s_initialized)
    {
        return LORAWAN_UPLINK_ERROR_NOT_READY;
    }

    if (s_queue_count >= BOLUS_LORAWAN_QUEUE_DEPTH)
    {
        lorawan_uplink_service_diag.queue_full_count++;
        return LORAWAN_UPLINK_ERROR_QUEUE_FULL;
    }

    entry = &s_queue[s_queue_tail];
    memset(entry, 0, sizeof(*entry));
    memcpy(entry->payload, payload, size);
    entry->size = size;
    entry->sequence = sequence;
    entry->valid = true;

    s_queue_tail = (uint8_t)((s_queue_tail + 1U) % BOLUS_LORAWAN_QUEUE_DEPTH);
    s_queue_count++;

    lorawan_uplink_service_diag.submit_count++;
    lorawan_uplink_service_diag.queue_count = s_queue_count;
    lorawan_uplink_service_diag.last_sequence_submitted = sequence;

    return LORAWAN_UPLINK_OK;
}

void LoRaWanUplinkService_Process(uint32_t now_ms)
{
    if (!s_initialized)
    {
        return;
    }

    /* DIO handlers may access SPI, so execute them only in cooperative context. */
    RFM95W_Board_ProcessIrqs();

    /* Process MAC state, RX1/RX2 timers, joins, confirms and indications. */
    LoRaMacProcess();
    lorawan_uplink_service_diag.mac_process_pending = false;

    if (!lorawan_uplink_service_diag.credentials_provisioned)
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_WAIT_CREDENTIALS;
        return;
    }

    if (!TimeReached(now_ms, s_next_action_tick_ms))
    {
        return;
    }

    if (!s_joined)
    {
        TryJoin(now_ms);
        return;
    }

    if (!s_tx_in_flight && (s_queue_count > 0U))
    {
        TrySendHead(now_ms);
    }
    else if (!s_tx_in_flight)
    {
        lorawan_uplink_service_diag.state = LORAWAN_UPLINK_STATE_JOINED_IDLE;
    }
}

bool LoRaWanUplinkService_IsReady(void)
{
    return s_initialized;
}

bool LoRaWanUplinkService_CanAccept(void)
{
    return (s_initialized && (s_queue_count < BOLUS_LORAWAN_QUEUE_DEPTH));
}

bool LoRaWanUplinkService_IsJoined(void)
{
    return (s_initialized && s_joined);
}
