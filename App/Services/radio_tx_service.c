#include "radio_tx_service.h"

#include "bolus_config.h"
#include "fault_manager.h"
#include "rfm95w_board.h"

#include <string.h>

volatile radio_tx_service_diag_t radio_tx_service_diag = {0};

static bolus_radio_config_t s_radio_config = {0};
static uint8_t s_pending_payload[RADIO_TX_SERVICE_MAX_PAYLOAD_SIZE] = {0};

/* Callbacks set only flags. All service state transitions remain in main context. */
static volatile bool s_tx_done_callback_pending = false;
static volatile bool s_tx_timeout_callback_pending = false;

static bool TimeReached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0);
}

static void RadioTxService_OnTxDone(void)
{
    s_tx_done_callback_pending = true;
}

static void RadioTxService_OnTxTimeout(void)
{
    s_tx_timeout_callback_pending = true;
}

void RadioTxService_AttachEvents(RadioEvents_t *events)
{
    if (events == NULL)
    {
        return;
    }

    events->TxDone = RadioTxService_OnTxDone;
    events->TxTimeout = RadioTxService_OnTxTimeout;
}

static void ConfigureForCurrentAttempt(void)
{
    /*
     * Raw LoRa staging transport. Public sync word is selected because the
     * eventual deployment target is LoRaWAN/public-network hardware, but the
     * 32-byte V2 buffer is NOT a valid LoRaWAN PHYPayload yet.
     */
    Radio.SetModem(MODEM_LORA);
    Radio.SetPublicNetwork(true);
    Radio.SetChannel(RFM95W_DEFAULT_FREQUENCY_HZ);

    Radio.SetTxConfig(
        MODEM_LORA,
        s_radio_config.tx_power_dbm,
        0U,
        s_radio_config.bandwidth_index,
        s_radio_config.spreading_factor,
        s_radio_config.coding_rate,
        RFM95W_DEFAULT_PREAMBLE_LENGTH,
        false,
        true,
        false,
        0U,
        false,
        s_radio_config.tx_timeout_ms);

    Radio.SetMaxPayloadLength(MODEM_LORA, RADIO_TX_SERVICE_MAX_PAYLOAD_SIZE);

    radio_tx_service_diag.last_air_time_ms =
        Radio.TimeOnAir(
            MODEM_LORA,
            s_radio_config.bandwidth_index,
            s_radio_config.spreading_factor,
            s_radio_config.coding_rate,
            RFM95W_DEFAULT_PREAMBLE_LENGTH,
            false,
            radio_tx_service_diag.pending_size,
            true);
}

static void DropCurrentPacket(radio_tx_result_t result)
{
    radio_tx_service_diag.last_result = result;
    radio_tx_service_diag.dropped_count++;
    radio_tx_service_diag.pending_valid = false;
    radio_tx_service_diag.pending_size = 0U;
    radio_tx_service_diag.pending_sequence = 0U;
    radio_tx_service_diag.attempts_for_current = 0U;
    radio_tx_service_diag.retry_due_ms = 0U;
    radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;
}

static void ScheduleRetryOrDrop(
    uint32_t now_ms,
    radio_tx_result_t drop_result)
{
    if (radio_tx_service_diag.attempts_for_current <
        s_radio_config.max_tx_attempts)
    {
        radio_tx_service_diag.retry_count++;
        radio_tx_service_diag.retry_due_ms =
            now_ms + (uint32_t)s_radio_config.retry_delay_ms;
        radio_tx_service_diag.state = RADIO_TX_STATE_RETRY_WAIT;
    }
    else
    {
        DropCurrentPacket(drop_result);
    }
}

static void HandleSuccessfulTx(uint32_t now_ms)
{
    Radio.Sleep();

    radio_tx_service_diag.last_tx_duration_ms =
        now_ms - radio_tx_service_diag.last_tx_start_ms;
    radio_tx_service_diag.tx_done_count++;
    radio_tx_service_diag.last_result = RADIO_TX_RESULT_SUCCESS;
    radio_tx_service_diag.pending_valid = false;
    radio_tx_service_diag.pending_size = 0U;
    radio_tx_service_diag.attempts_for_current = 0U;
    radio_tx_service_diag.retry_due_ms = 0U;
    radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;

    (void)FaultManager_ClearFault(BOLUS_FAULT_RF_TX_TIMEOUT);
    (void)FaultManager_ClearFault(BOLUS_FAULT_RF_COMM);
}

static void HandleTxTimeout(uint32_t now_ms)
{
    Radio.Sleep();
    radio_tx_service_diag.tx_timeout_count++;
    FaultManager_Raise(BOLUS_FAULT_RF_TX_TIMEOUT);
    ScheduleRetryOrDrop(now_ms, RADIO_TX_RESULT_DROPPED_TIMEOUT);
}

static void HandleRfError(uint32_t now_ms)
{
    Radio.Sleep();
    radio_tx_service_diag.rf_error_count++;
    FaultManager_Raise(BOLUS_FAULT_RF_COMM);
    ScheduleRetryOrDrop(now_ms, RADIO_TX_RESULT_DROPPED_RF_ERROR);
}

static void StartAttempt(uint32_t now_ms)
{
    if ((!radio_tx_service_diag.pending_valid) ||
        (radio_tx_service_diag.pending_size == 0U))
    {
        radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;
        return;
    }

    if (Radio.GetStatus() != RF_IDLE)
    {
        return;
    }

    /* Reconfigure every attempt: the Semtech timeout recovery path resets radio. */
    ConfigureForCurrentAttempt();

    if (RFM95W_Board_GetLastSpiStatus() != HAL_OK)
    {
        HandleRfError(now_ms);
        return;
    }

    radio_tx_service_diag.attempts_for_current++;
    radio_tx_service_diag.tx_start_count++;
    radio_tx_service_diag.last_tx_start_ms = now_ms;
    radio_tx_service_diag.state = RADIO_TX_STATE_TX_RUNNING;

    Radio.Send(s_pending_payload, radio_tx_service_diag.pending_size);

    if (RFM95W_Board_GetLastSpiStatus() != HAL_OK)
    {
        HandleRfError(now_ms);
    }
}

radio_tx_service_status_t RadioTxService_Init(
    const bolus_runtime_config_t *config)
{
    if (config == NULL)
    {
        return RADIO_TX_SERVICE_ERROR_PARAM;
    }

    if (!BolusRuntimeConfig_Validate(config))
    {
        FaultManager_Raise(BOLUS_FAULT_CONFIG_INVALID);
        return RADIO_TX_SERVICE_ERROR_CONFIG;
    }

    memset((void *)&radio_tx_service_diag, 0, sizeof(radio_tx_service_diag));
    memset(s_pending_payload, 0, sizeof(s_pending_payload));

    s_radio_config = config->radio;
    s_tx_done_callback_pending = false;
    s_tx_timeout_callback_pending = false;

    /* Driver must already have completed SX1276Init()/Radio.Init(). */
    if (Radio.GetStatus() != RF_IDLE)
    {
        return RADIO_TX_SERVICE_ERROR_RF;
    }

    radio_tx_service_diag.initialized = true;
    radio_tx_service_diag.state = RADIO_TX_STATE_IDLE;
    radio_tx_service_diag.last_result = RADIO_TX_RESULT_NONE;

    (void)FaultManager_ClearFault(BOLUS_FAULT_RF_COMM);
    (void)FaultManager_ClearFault(BOLUS_FAULT_RF_TX_TIMEOUT);

    return RADIO_TX_SERVICE_OK;
}

radio_tx_service_status_t RadioTxService_Submit(
    const uint8_t *payload,
    uint8_t size,
    uint16_t sequence)
{
    if ((payload == NULL) || (size == 0U) ||
        (size > RADIO_TX_SERVICE_MAX_PAYLOAD_SIZE))
    {
        return RADIO_TX_SERVICE_ERROR_PARAM;
    }

    if (!radio_tx_service_diag.initialized)
    {
        return RADIO_TX_SERVICE_ERROR_NOT_READY;
    }

    if (radio_tx_service_diag.pending_valid ||
        (radio_tx_service_diag.state != RADIO_TX_STATE_IDLE))
    {
        radio_tx_service_diag.busy_reject_count++;
        return RADIO_TX_SERVICE_BUSY;
    }

    memcpy(s_pending_payload, payload, size);

    radio_tx_service_diag.pending_valid = true;
    radio_tx_service_diag.pending_size = size;
    radio_tx_service_diag.pending_sequence = sequence;
    radio_tx_service_diag.attempts_for_current = 0U;
    radio_tx_service_diag.retry_due_ms = 0U;
    radio_tx_service_diag.last_result = RADIO_TX_RESULT_NONE;
    radio_tx_service_diag.submit_count++;
    radio_tx_service_diag.state = RADIO_TX_STATE_PENDING;

    return RADIO_TX_SERVICE_OK;
}

void RadioTxService_Process(uint32_t now_ms)
{
    if (!radio_tx_service_diag.initialized)
    {
        return;
    }

    /*
     * DIO callbacks are deliberately deferred out of the EXTI ISR. This call
     * executes the stored Semtech DIO handlers in cooperative main context.
     */
    RFM95W_Board_ProcessIrqs();

    if (s_tx_done_callback_pending)
    {
        s_tx_done_callback_pending = false;

        if (radio_tx_service_diag.state == RADIO_TX_STATE_TX_RUNNING)
        {
            HandleSuccessfulTx(now_ms);
        }
    }

    if (s_tx_timeout_callback_pending)
    {
        s_tx_timeout_callback_pending = false;

        if (radio_tx_service_diag.state == RADIO_TX_STATE_TX_RUNNING)
        {
            HandleTxTimeout(now_ms);
        }
    }

    /*
     * Secondary software deadline protects the service if a driver timeout
     * callback is ever missed. It is intentionally slightly later than the
     * SX1276 timer configured by SetTxConfig().
     */
    if ((radio_tx_service_diag.state == RADIO_TX_STATE_TX_RUNNING) &&
        TimeReached(
            now_ms,
            radio_tx_service_diag.last_tx_start_ms +
            (uint32_t)s_radio_config.tx_timeout_ms + 250UL))
    {
        HandleTxTimeout(now_ms);
    }

    if (radio_tx_service_diag.state == RADIO_TX_STATE_PENDING)
    {
        StartAttempt(now_ms);
    }
    else if ((radio_tx_service_diag.state == RADIO_TX_STATE_RETRY_WAIT) &&
             TimeReached(now_ms, radio_tx_service_diag.retry_due_ms))
    {
        StartAttempt(now_ms);
    }
}

bool RadioTxService_IsReady(void)
{
    return radio_tx_service_diag.initialized;
}

bool RadioTxService_CanAccept(void)
{
    return radio_tx_service_diag.initialized &&
           (!radio_tx_service_diag.pending_valid) &&
           (radio_tx_service_diag.state == RADIO_TX_STATE_IDLE);
}

bool RadioTxService_IsBusy(void)
{
    return radio_tx_service_diag.initialized &&
           ((radio_tx_service_diag.state != RADIO_TX_STATE_IDLE) ||
            radio_tx_service_diag.pending_valid);
}
