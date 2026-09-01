# Phase 5 LoRaWAN Uplink Management — staging checkpoint

Status: **IMPLEMENTED / UNTESTED**

This checkpoint replaces the raw-LoRa transport policy with a LoRaWAN-aware uplink manager while preserving the validated TelemetryWindow ownership handoff.

## Scope

- I-CUBE-LRWAN / LoRaMac 4.4.7 direct integration (LmHandler is not required for this minimal path).
- Region: EU868 only.
- Device class: Class A.
- Activation: OTAA.
- ADR: enabled by default.
- Application uplink: unconfirmed by default, FPort 2.
- Telemetry packet: existing fixed 32-byte Telemetry V2 payload.
- Queue: two copied application packets, so the next TelemetryWindow freeze cannot overwrite an in-flight packet.
- LoRaMAC owns frame construction, MIC, encryption, frame counters, duty-cycle scheduling and RX1/RX2 timing.
- Service records RX1/RX2 downlink metadata for the next Downlink Management milestone, but does not decode/apply commands yet.

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
                              queued telemetry
                                        |
                                        v
                                  TX_IN_FLIGHT
                                  /           \
                            success           failure
                              |                  |
                              v                  v
                         dequeue            RETRY_WAIT
                                                |
                                      bounded retry policy
```

Duty-cycle restriction and MAC-busy responses defer transmission without consuming an RF attempt.

## Diagnostics

Authoritative debugger structure:

`lorawan_uplink_service_diag`

Important fields:

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
- `last_downlink_port`
- `last_downlink_size`
- `last_downlink_rssi_dbm`
- `last_downlink_snr_db`
- `last_downlink_slot`

The older `radio_tx_service_diag` remains only as a compatibility/debug mirror because `main.c` keeps the previously validated TelemetryWindow -> RadioTxService handoff.

## CubeIDE build staging

The repository keeps the previously known-good `.cproject` unchanged. Before the first LoRaWAN build, run from the repository root:

```bash
python tools/enable_lorawan_cubeide.py
```

The script only adds the required LoRaWAN Debug include/source paths and is idempotent. Then refresh the project, clean, and build Debug.

## Deliberately deferred

- Real OTAA credentials and network join validation.
- Actual gateway/network-server uplink validation.
- Downlink command decoding, validation, atomic config apply, ACK/NACK.
- LoRaMAC NVM/session persistence across resets.
- STOP2/RTC/LPTIM timebase migration.
- Radio rail power gating/current optimization.
- Production RF/antenna validation.

Do not mark this checkpoint PASS until a clean CubeIDE build and hardware validation are recorded.
