#ifndef RADIO_TX_SERVICE_H
#define RADIO_TX_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "../Config/bolus_runtime_config.h"
#include "radio.h"

#define RADIO_TX_SERVICE_MAX_PAYLOAD_SIZE  64U

typedef enum
{
    RADIO_TX_SERVICE_OK = 0,
    RADIO_TX_SERVICE_ERROR_PARAM,
    RADIO_TX_SERVICE_ERROR_CONFIG,
    RADIO_TX_SERVICE_ERROR_NOT_READY,
    RADIO_TX_SERVICE_BUSY,
    RADIO_TX_SERVICE_ERROR_RF
} radio_tx_service_status_t;

typedef enum
{
    RADIO_TX_STATE_UNINITIALIZED = 0,
    RADIO_TX_STATE_IDLE,
    RADIO_TX_STATE_PENDING,
    RADIO_TX_STATE_TX_RUNNING,
    RADIO_TX_STATE_RETRY_WAIT
} radio_tx_state_t;

typedef enum
{
    RADIO_TX_RESULT_NONE = 0,
    RADIO_TX_RESULT_SUCCESS,
    RADIO_TX_RESULT_DROPPED_TIMEOUT,
    RADIO_TX_RESULT_DROPPED_RF_ERROR
} radio_tx_result_t;

typedef struct
{
    bool initialized;
    radio_tx_state_t state;
    radio_tx_result_t last_result;

    bool pending_valid;
    uint8_t pending_size;
    uint16_t pending_sequence;
    uint8_t attempts_for_current;

    uint32_t submit_count;
    uint32_t busy_reject_count;
    uint32_t tx_start_count;
    uint32_t tx_done_count;
    uint32_t tx_timeout_count;
    uint32_t rf_error_count;
    uint32_t retry_count;
    uint32_t dropped_count;

    uint32_t last_tx_start_ms;
    uint32_t last_tx_duration_ms;
    uint32_t retry_due_ms;
    uint32_t last_air_time_ms;
} radio_tx_service_diag_t;

/* Live Expressions-friendly staging diagnostics. */
extern volatile radio_tx_service_diag_t radio_tx_service_diag;

/*
 * Must be called before SX1276Init()/Radio.Init() so the Semtech driver retains
 * our TxDone and TxTimeout callback pointers. Existing RX callbacks are left
 * untouched for the later downlink stage.
 */
void RadioTxService_AttachEvents(RadioEvents_t *events);

/*
 * Initialize the TX policy after the Phase-4 radio bring-up/regression path has
 * initialized the SX1276 driver. The service owns packet lifetime/retry policy,
 * not radio power gating; RFM95W rail optimization remains a later power stage.
 *
 * IMPORTANT: this is raw LoRa PHY transport management only. It does NOT create
 * a LoRaWAN PHYPayload, join a network, or implement RX1/RX2 yet.
 */
radio_tx_service_status_t RadioTxService_Init(
    const bolus_runtime_config_t *config);

/*
 * Copy one packet into service-owned RAM. A copied packet cannot be overwritten
 * by the next TelemetryWindow freeze while it is pending/in flight.
 */
radio_tx_service_status_t RadioTxService_Submit(
    const uint8_t *payload,
    uint8_t size,
    uint16_t sequence);

/* Cooperative main-context processing: deferred radio DIO work, TxDone/timeout,
 * bounded retry and the next send attempt. Call after TimerProcess(). */
void RadioTxService_Process(uint32_t now_ms);

bool RadioTxService_IsReady(void);
bool RadioTxService_CanAccept(void);
bool RadioTxService_IsBusy(void);

#endif /* RADIO_TX_SERVICE_H */
