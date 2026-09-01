# Phase 5 LoRaWAN Uplink Management — staging checkpoint

Status: **IMPLEMENTED / UNTESTED**

This checkpoint replaces the raw-LoRa transport policy with a LoRaWAN-aware uplink manager while preserving the validated TelemetryWindow ownership handoff.

The LoRaWAN uplink/downlink integration in this checkpoint is still **not build-validated, not gateway-validated, and not hardware-validated**.

## Scope

- I-CUBE-LRWAN / LoRaMac 4.4.7 direct integration (LmHandler is not required for this minimal path).
- Region: EU868 only.
- Device class: Class A.
- Activation: OTAA.
- ADR: enabled by default.
- Application telemetry uplink: unconfirmed by default, FPort 2.
- Configuration downlink staging: FPort 3.
- ACK/NACK control uplink staging: FPort 4.
- Telemetry packet: existing fixed 32-byte Telemetry V2 payload.
- Queue: two copied application packets, so the next TelemetryWindow freeze cannot overwrite an in-flight packet.
- Dedicated priority control-response slot for downlink ACK/NACK.
- LoRaMAC owns frame construction, MIC, encryption, frame counters, duty-cycle scheduling and RX1/RX2 timing.
- RX1/RX2 application commands are now routed into the separate Downlink Management staging module.

See `docs/phase5_lorawan_downlink_staging.md` for the downlink packet contract, command IDs, pending-apply semantics and explicit test boundary.

## Credential safety

`App/Config/bolus_lorawan_credentials.h` intentionally contains only zero placeholders and
`BOLUS_LORAWAN_CREDENTIALS_PROVISIONED == 0`.

Therefore this checkpoint must **not transmit OTAA Join Requests** until real credentials are provisioned through a private/local path. Production credentials must not be committed to this repository.

## Uplink state machine

```text
UNINITIALIZED
    |
    v
WAIT_CREDENTIALS  -- credentials provisioned --> JOIN_WAIT
                                                |
                                                v
                                             JOINING
                                            /       \
                                      success       failure
                                        |              |
                                        v              v
                                  JOINED_IDLE       JOIN_WAIT
                                        |
                      priority ACK/NACK or telemetry
                                        |
                                        v
                                  TX_IN_FLIGHT
                                  /           \
                            success           failure
                              |                  |
                              v                  v
                         complete            RETRY_WAIT
                                                |
                                      bounded retry policy
```

Duty-cycle restriction and MAC-busy responses defer transmission without consuming an RF attempt.

## Diagnostics

Authoritative LoRaWAN debugger structure:

`lorawan_uplink_service_diag`

Important fields include:

- `initialized`
- `credentials_provisioned`
- `state`
- `joined`
- `join_in_flight`
- `tx_in_flight`
- `queue_count`
- `submit_count`
- `join_request_count`
- `join_success_count`
- `join_failure_count`
- `tx_request_count`
- `tx_success_count`
- `tx_failure_count`
- `tx_retry_count`
- `tx_drop_count`
- `duty_cycle_defer_count`
- `mac_busy_defer_count`
- `last_mac_status`
- `last_mcps_confirm_status`
- `last_join_confirm_status`
- `downlink_count`
- `downlink_command_count`
- `downlink_wrong_port_count`
- `downlink_response_queued_count`
- `downlink_response_tx_success_count`
- `downlink_response_tx_failure_count`
- `downlink_response_drop_count`
- `last_downlink_port`
- `last_downlink_size`
- `last_downlink_rssi_dbm`
- `last_downlink_snr_db`
- `last_downlink_slot`

Downlink parsing/config diagnostics are separately exposed in:

`downlink_management_diag`

and remain explicitly marked `DOWNLINK_VALIDATION_UNTESTED`.

The older `radio_tx_service_diag` remains only as a compatibility/debug mirror because `main.c` keeps the previously validated TelemetryWindow -> RadioTxService handoff.

## CubeIDE build staging

The repository keeps the previously known-good `.cproject` unchanged. Before the first LoRaWAN build, run from the repository root:

```bash
python tools/enable_lorawan_cubeide.py
```

The script only adds the required LoRaWAN Debug include/source paths and is idempotent. Then refresh the project, clean, and build Debug.

## Deliberately deferred / not validated

- Real OTAA credentials and network join validation.
- Actual gateway/network-server telemetry uplink validation.
- Actual FPort-3 downlink and FPort-4 ACK/NACK validation.
- Live reconfiguration of cached BMA/EventEpisode/MPU/TelemetryWindow settings after a valid downlink.
- Config persistence across reset/power loss.
- LoRaMAC NVM/session persistence across resets.
- STOP2/RTC/LPTIM timebase migration.
- Radio rail power gating/current optimization.
- Production RF/antenna validation.

Do not mark this checkpoint PASS until a clean CubeIDE build and hardware/network validation are recorded.
