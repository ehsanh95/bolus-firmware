# Phase 5 — Managed Telemetry TX V1

Date: 2026-09-01
Branch: `phase5/application-architecture`

## Status

**IMPLEMENTED / UNTESTED**

This stage connects the already prepared 32-byte Telemetry V2 snapshot to the existing RFM95W/SX1276 driver through a bounded, cooperative transmission manager. It has not yet been clean-built/flashed/validated on hardware after the new TX changes.

## Important protocol boundary

The repository currently contains the Semtech/ST SX1276 radio driver but no LoRaWAN MAC/network stack. Therefore this stage is **raw LoRa PHY transport management**, not a valid LoRaWAN uplink implementation.

The current 32-byte Telemetry V2 buffer is copied directly into the SX1276 FIFO. It is **not** a LoRaWAN PHYPayload and does not implement Join, session keys, frame counters, MIC, RX1/RX2 or network-server acknowledgements.

A true LoRaWAN downlink stage will therefore require MAC/network integration before command/config downlink can be considered deployment-ready.

## RuntimeConfig v11 radio policy

| Parameter | Default | Validation |
|---|---:|---:|
| uplink period | 900 s | 1..86400 s |
| TX power | 10 dBm | 2..20 dBm |
| spreading factor | SF7 | SF7..SF12 |
| bandwidth index | 0 = 125 kHz | 0..2 |
| coding rate | 1 = 4/5 | 1..4 |
| TX timeout | 3000 ms | 500..10000 ms |
| retry delay | 2000 ms | 0..60000 ms |
| maximum TX attempts | 3 | 1..5 |

## Packet ownership state machine

`TelemetryWindow freeze -> V2 encode -> RadioTxService_Submit(copy) -> PENDING -> TX_RUNNING -> TxDone`

On success:

`TxDone -> radio sleep -> clear pending packet -> IDLE`

On timeout/RF error:

`TX failure -> radio sleep -> RF fault -> RETRY_WAIT -> retry after 2 s`

After the third failed attempt:

`drop current packet -> dropped_count++ -> IDLE`

Only one telemetry packet is owned by the TX service at a time. `Submit()` copies the packet into service-owned RAM, so the source Telemetry V2 buffer cannot overwrite the packet while it is pending or in flight.

## Interrupt design

RFM95W DIO0 is PA10 / EXTI15_10 and is used for LoRa TxDone.

The EXTI path is intentionally minimal:

1. `EXTI15_10_IRQHandler()` calls the HAL GPIO EXTI handler.
2. `HAL_GPIO_EXTI_Callback()` records a pending DIO bit only.
3. `RadioTxService_Process()` calls `RFM95W_Board_ProcessIrqs()` in main context.
4. The stored Semtech DIO handler then reads/clears SX1276 IRQ registers and raises the TxDone callback.

This keeps SPI transactions out of the ISR and preserves the same ISR-minimality principle used for BMA456 INT1.

DIO1/DIO2 remain ordinary inputs until the RX/downlink stage.

## Expected debugger values immediately after boot

- `sensor_service_config.version = 11`
- `radio_tx_service_ready = true`
- `radio_tx_service_diag.initialized = true`
- `radio_tx_service_diag.state = RADIO_TX_STATE_IDLE`
- `radio_tx_service_diag.pending_valid = false`
- `radio_tx_service_diag.tx_done_count = 0`
- `radio_tx_service_diag.tx_timeout_count = 0`
- `rfm95w_final_ok = true`

## Normal TX test

To avoid waiting 15 minutes during the bench test, `telemetry_window_service.uplink_period_ms` may be temporarily changed in the debugger to `30000` for a 30-second staging window. This changes only the live RAM service interval; it does not modify RuntimeConfig or the committed 900-second product default.

Expected sequence after one window expires:

1. `telemetry_snapshot_count` increments.
2. Telemetry V2 is encoded as 32 bytes.
3. `telemetry_payload_v2_queued_count` increments.
4. `radio_tx_service_diag.submit_count` increments.
5. `radio_tx_service_diag.tx_start_count` increments.
6. DIO0/TxDone completes the local RF transmission.
7. `radio_tx_service_diag.tx_done_count` increments.
8. `radio_tx_service_diag.last_result = RADIO_TX_RESULT_SUCCESS`.
9. `radio_tx_service_diag.pending_valid = false`.
10. state returns to `RADIO_TX_STATE_IDLE`.

Note: `telemetry_payload_v2_ready` now becomes false after successful **queue ownership transfer**, not after RF TxDone. This is expected because the TX service owns its private copy from that point onward.

## What a TxDone PASS proves

A local TxDone PASS proves that the SX1276 completed an on-air raw LoRa transmission and the DIO0/timeout/ownership state machine worked.

It does **not** prove:

- gateway reception,
- LoRaWAN network acceptance,
- frame integrity at the application server,
- confirmed-uplink acknowledgement,
- RX1/RX2 downlink behavior.

Those are separate integration tests.

## Next stage after TX validation

After the managed TX state machine is hardware-validated, proceed to Downlink Management in two layers:

1. **Network/RX layer** — integrate the intended LoRaWAN MAC/network behavior and RX windows.
2. **Command/config layer** — message envelope, parser, validation, atomic config apply, ACK/NACK, versioning and persistence policy.

Power/STOP2/RTC/leakage optimization remains intentionally after TX and downlink functionality are complete, as requested.
