#ifndef LORAWAN_UPLINK_SERVICE_H
#define LORAWAN_UPLINK_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "bolus_runtime_config.h"
#include "LoRaMac.h"

/*
 * [UNTESTED] LoRaWAN ABP network integration staging.
 * A clean build plus gateway/TTN validation is still required after changes.
 */

typedef enum
{
    LORAWAN_UPLINK_STATE_UNINITIALIZED = 0,
    LORAWAN_UPLINK_STATE_WAIT_CREDENTIALS,
    LORAWAN_UPLINK_STATE_JOIN_WAIT,
    LORAWAN_UPLINK_STATE_JOINING,
    LORAWAN_UPLINK_STATE_JOINED_IDLE,
    LORAWAN_UPLINK_STATE_TX_IN_FLIGHT,
    LORAWAN_UPLINK_STATE_RETRY_WAIT,
    LORAWAN_UPLINK_STATE_ERROR
} lorawan_uplink_state_t;

typedef enum
{
    LORAWAN_UPLINK_OK = 0,
    LORAWAN_UPLINK_ERROR_PARAM,
    LORAWAN_UPLINK_ERROR_CONFIG,
    LORAWAN_UPLINK_ERROR_MAC_INIT,
    LORAWAN_UPLINK_ERROR_MAC_START,
    LORAWAN_UPLINK_ERROR_MIB,
    LORAWAN_UPLINK_ERROR_QUEUE_FULL,
    LORAWAN_UPLINK_ERROR_NOT_READY
} lorawan_uplink_status_t;

typedef struct
{
    bool initialized;
    bool credentials_provisioned;
    bool joined;
    bool join_in_flight;
    bool tx_in_flight;
    bool mac_process_pending;
    lorawan_uplink_state_t state;

    /* ABP activation diagnostics. */
    ActivationType_t activation_mode;
    bool abp_session_configured;
    bool abp_network_keys_match;
    uint32_t abp_session_configure_count;
    uint32_t abp_session_configure_failure_count;
    uint32_t abp_dev_addr;

    uint8_t queue_count;
    uint8_t queue_capacity;
    uint8_t app_port;

    uint32_t submit_count;
    uint32_t queue_full_count;

    /* Legacy OTAA counters retained for debugger/layout continuity; ABP keeps them at zero. */
    uint32_t join_request_count;
    uint32_t join_success_count;
    uint32_t join_failure_count;

    uint32_t tx_request_count;
    uint32_t tx_success_count;
    uint32_t tx_failure_count;
    uint32_t tx_retry_count;
    uint32_t tx_drop_count;
    uint32_t tx_watchdog_timeout_count;
    uint32_t tx_watchdog_recovery_count;
    uint32_t tx_watchdog_recovery_failure_count;
    uint32_t late_mcps_confirm_count;
    uint32_t duty_cycle_defer_count;
    uint32_t mac_busy_defer_count;
    uint32_t nvm_change_count;

    uint32_t last_sequence_submitted;
    uint32_t last_sequence_completed;
    uint32_t last_uplink_counter;
    uint32_t last_tx_air_time_ms;
    uint32_t tx_watchdog_deadline_ms;
    uint32_t next_action_tick_ms;

    LoRaMacStatus_t last_mac_status;
    LoRaMacEventInfoStatus_t last_mcps_confirm_status;
    LoRaMacEventInfoStatus_t last_join_confirm_status;

    /* RX1/RX2 observations plus staged [UNTESTED] command routing. */
    uint32_t downlink_count;
    uint32_t downlink_command_count;
    uint32_t downlink_wrong_port_count;
    uint32_t downlink_response_queued_count;
    uint32_t downlink_response_tx_success_count;
    uint32_t downlink_response_tx_failure_count;
    uint32_t downlink_response_drop_count;

    uint32_t last_downlink_counter;
    uint8_t last_downlink_port;
    uint8_t last_downlink_size;
    int16_t last_downlink_rssi_dbm;
    int8_t last_downlink_snr_db;
    LoRaMacRxSlot_t last_downlink_slot;
} lorawan_uplink_diag_t;

extern lorawan_uplink_diag_t lorawan_uplink_service_diag;

/*
 * RuntimeConfig is intentionally mutable because accepted downlink commands are
 * atomically committed into this RAM object. Cached services still expose a
 * pending-apply mask and are NOT claimed as live-reconfigured yet.
 */
lorawan_uplink_status_t LoRaWanUplinkService_Init(
    bolus_runtime_config_t *config);

lorawan_uplink_status_t LoRaWanUplinkService_Submit(
    const uint8_t *payload,
    uint8_t size,
    uint16_t sequence);

void LoRaWanUplinkService_Process(uint32_t now_ms);

bool LoRaWanUplinkService_IsReady(void);
bool LoRaWanUplinkService_CanAccept(void);
bool LoRaWanUplinkService_IsJoined(void);

#endif /* LORAWAN_UPLINK_SERVICE_H */
