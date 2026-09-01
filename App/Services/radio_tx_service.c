#include "radio_tx_service.h"

#include "lorawan_uplink_service.h"

#include <string.h>

/*
 * Compatibility facade.
 *
 * main.c already owns a stable RadioTxService handoff contract from the raw
 * LoRa staging milestone. Keep that contract while routing the packet through
 * LoRaMAC instead of calling SX1276Send() directly. This avoids changing the
 * validated telemetry ownership path while the network layer is introduced.
 *
 * [UNTESTED] The LoRaWAN uplink/downlink path behind this facade has not yet
 * passed clean-build or network/hardware validation.
 */
volatile radio_tx_service_diag_t radio_tx_service_diag = {0};

static bool s_ready = false;

void RadioTxService_AttachEvents(RadioEvents_t *events)
{
    if (events == NULL)
    {
        return;
    }

    /*
     * Phase-4 startup diagnostics still call SX1276Init() before LoRaMAC is
     * initialized. Leave those callbacks empty. LoRaMacInitialization() later
     * calls Radio.Init() again and installs the MAC-owned Tx/Rx callbacks that
     * are required for TX, RX1 and RX2.
     */
    memset(events, 0, sizeof(*events));
}

radio_tx_service_status_t RadioTxService_Init(
    bolus_runtime_config_t *config)
{
    lorawan_uplink_status_t status;

    memset((void *)&radio_tx_service_diag, 0, sizeof(radio_tx_service_diag));
    s_ready = false;

    status = LoRaWanUplinkService_Init(config);
    if (status != LORAWAN_UPLINK_OK)
    {
        radio_tx_service_diag.state = RADIO_TX_STATE_UNINITIALIZED;
        radio_tx_service_diag.rf_error_count++;
        return RADIO_TX_SERVICE_ERROR_RF;
    }

    s_ready = true;
    radio_tx_service_diag.initialized = true;
    radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;
    return RADIO_TX_SERVICE_OK;
}

radio_tx_service_status_t RadioTxService_Submit(
    const uint8_t *payload,
    uint8_t size,
    uint16_t sequence)
{
    lorawan_uplink_status_t status;

    if ((payload == NULL) || (size == 0U) ||
        (size > RADIO_TX_SERVICE_MAX_PAYLOAD_SIZE))
    {
        return RADIO_TX_SERVICE_ERROR_PARAM;
    }

    if (!s_ready)
    {
        return RADIO_TX_SERVICE_ERROR_NOT_READY;
    }

    status = LoRaWanUplinkService_Submit(payload, size, sequence);
    if (status == LORAWAN_UPLINK_ERROR_QUEUE_FULL)
    {
        radio_tx_service_diag.busy_reject_count++;
        return RADIO_TX_SERVICE_BUSY;
    }

    if (status != LORAWAN_UPLINK_OK)
    {
        radio_tx_service_diag.rf_error_count++;
        return RADIO_TX_SERVICE_ERROR_RF;
    }

    radio_tx_service_diag.submit_count++;
    radio_tx_service_diag.pending_valid = true;
    radio_tx_service_diag.pending_size = size;
    radio_tx_service_diag.pending_sequence = sequence;
    radio_tx_service_diag.state = RADIO_TX_STATE_PENDING;
    return RADIO_TX_SERVICE_OK;
}

void RadioTxService_Process(uint32_t now_ms)
{
    if (!s_ready)
    {
        return;
    }

    LoRaWanUplinkService_Process(now_ms);

    /* Mirror high-value LoRaWAN diagnostics into the old debugger structure. */
    radio_tx_service_diag.pending_valid =
        (lorawan_uplink_service_diag.queue_count > 0U);
    radio_tx_service_diag.attempts_for_current =
        (uint8_t)lorawan_uplink_service_diag.tx_request_count;
    radio_tx_service_diag.tx_start_count =
        lorawan_uplink_service_diag.tx_request_count;
    radio_tx_service_diag.tx_done_count =
        lorawan_uplink_service_diag.tx_success_count;
    radio_tx_service_diag.tx_timeout_count =
        lorawan_uplink_service_diag.tx_failure_count;
    radio_tx_service_diag.retry_count =
        lorawan_uplink_service_diag.tx_retry_count;
    radio_tx_service_diag.dropped_count =
        lorawan_uplink_service_diag.tx_drop_count;
    radio_tx_service_diag.retry_due_ms =
        lorawan_uplink_service_diag.next_action_tick_ms;
    radio_tx_service_diag.last_air_time_ms =
        lorawan_uplink_service_diag.last_tx_air_time_ms;

    if (lorawan_uplink_service_diag.tx_in_flight)
    {
        radio_tx_service_diag.state = RADIO_TX_STATE_TX_RUNNING;
    }
    else if (lorawan_uplink_service_diag.queue_count > 0U)
    {
        if (lorawan_uplink_service_diag.state == LORAWAN_UPLINK_STATE_RETRY_WAIT)
        {
            radio_tx_service_diag.state = RADIO_TX_STATE_RETRY_WAIT;
        }
        else
        {
            radio_tx_service_diag.state = RADIO_TX_STATE_PENDING;
        }
    }
    else
    {
        radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;
    }

    if (lorawan_uplink_service_diag.tx_success_count > 0U)
    {
        radio_tx_service_diag.last_result = RADIO_TX_RESULT_SUCCESS;
        radio_tx_service_diag.pending_sequence =
            (uint16_t)lorawan_uplink_service_diag.last_sequence_completed;
    }
    else if (lorawan_uplink_service_diag.tx_drop_count > 0U)
    {
        radio_tx_service_diag.last_result = RADIO_TX_RESULT_DROPPED_RF_ERROR;
    }
}

bool RadioTxService_IsReady(void)
{
    return (s_ready && LoRaWanUplinkService_IsReady());
}

bool RadioTxService_CanAccept(void)
{
    return (s_ready && LoRaWanUplinkService_CanAccept());
}

bool RadioTxService_IsBusy(void)
{
    return (s_ready &&
            (lorawan_uplink_service_diag.tx_in_flight ||
             (lorawan_uplink_service_diag.queue_count > 0U)));
}
