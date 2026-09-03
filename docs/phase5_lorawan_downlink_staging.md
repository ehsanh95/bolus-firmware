# Phase 5 LoRaWAN Downlink Management — staging checkpoint

Status: **IMPLEMENTED / UNTESTED / NOT HARDWARE VALIDATED**

This status is intentional. The code exists in the branch, but it must **not** be called PASS until a clean CubeIDE build plus real OTAA/gateway/network-server/hardware tests are recorded.

## What is implemented

- LoRaWAN Class-A RX1/RX2 application downlinks are routed from `MacMcpsIndication`.
- Application command downlink uses **FPort 3**.
- ACK/NACK control uplink uses **FPort 4**.
- Telemetry uplink remains **FPort 2**.
- A dedicated priority control-response slot prevents an ACK/NACK from corrupting the telemetry ownership queue.
- Command frames use a versioned TLV protocol.
- Full-frame parsing is atomic: a temporary candidate RuntimeConfig is modified first, then `BolusRuntimeConfig_Validate()` validates the whole candidate, and only then is the RAM RuntimeConfig replaced.
- Duplicate successful transaction IDs are detected so a repeated command is not applied twice.
- Invalid magic/version/length/command/value/config combinations produce NACK results rather than partial configuration changes.
- Downlink and response diagnostics are exposed for Live Expressions.
- RF/radio RuntimeConfig fields are now addressable through separate downlink command IDs.
- A standalone offline generator exists at `Bolus_Downlink_Configurator/index.html` for selecting settings and generating FPort-3 HEX/Base64 payloads. The same page can decode the fixed FPort-4 ACK/NACK format.

## Important activation boundary

Accepted commands currently update the application-owned `bolus_runtime_config_t` atomically in RAM.

However, several existing services cache configuration during their initialization. Therefore a successful downlink does **not** mean every hardware/service parameter is already live.

`downlink_management_diag.pending_apply_mask` explicitly records which cached subsystems still require a live-reconfigure implementation.

Current apply-mask bits:

- bit 0 — BMA Any-Motion/Event service reconfiguration required
- bit 1 — BMA Sensor/Step service reconfiguration required
- bit 2 — EventEpisode policy reconfiguration required
- bit 3 — MPU service reconfiguration required
- bit 4 — TelemetryWindow period reconfiguration required
- bit 5 — Radio/LoRaWAN policy reconfiguration required

The TMP background sample period is read directly from RuntimeConfig by the existing main loop, so that one does not require a cached-service re-init.

**Do not clear `pending_apply_mask` or describe these settings as fully applied until the live-reconfiguration stage is implemented and tested.**

## RF boundary

The new RF commands modify the corresponding fields already present in `RuntimeConfig.radio`:

- TX power dBm
- spreading factor
- bandwidth index
- coding rate
- TX timeout
- retry delay
- maximum TX attempts

These commands are **implemented as atomic RuntimeConfig updates but remain UNTESTED and PENDING_APPLY**.

The imported EU868 LoRaWAN stack derives its actual LoRa PHY modulation from LoRaWAN data-rate/region rules. Therefore the current RF command support must not yet be described as verified live MAC/PHY reconfiguration. A later apply layer must map supported RuntimeConfig policy into LoRaMAC/RegionEU868 semantics and validate the result over the network.

The existing `BolusRuntimeConfig_Validate()` is still the final range validator. Current accepted RuntimeConfig ranges are:

- TX power: 2..20 dBm
- SF: 7..12
- bandwidth index: 0..2
- coding rate: 1..4
- TX timeout: 500..10000 ms
- retry delay: 0..60000 ms
- maximum TX attempts: 1..5

These ranges are configuration-contract ranges; they are not a statement that every combination is a standards-valid EU868 LoRaWAN PHY configuration.

## Downlink request packet

All multi-byte values are little-endian.

```text
byte 0    0xD1 request magic
byte 1    protocol version = 1
byte 2    transaction id
byte 3    TLV command count
byte 4+   [command id][length][value ...] repeated
```

No application CRC is added because the LoRaWAN frame already has MAC-layer integrity/MIC handling. This choice is part of the staging design and still requires end-to-end validation.

## Supported command IDs

| ID | Command | Length | RuntimeConfig field | Live apply status |
|---:|---|---:|---|---|
| `0x01` | BMA Event sensitivity profile | 1 | `event_processing.bma_event_sensitivity_level` | pending BMA Event apply |
| `0x02` | BMA Step sensitivity | 1 | `bma.step_sensitivity` | pending BMA Sensor apply |
| `0x03` | Episode retrigger guard, ms | 2 | `episode_retrigger_guard_ms` | pending Episode apply |
| `0x04` | Episode quiet timeout, s | 2 | `episode_quiet_timeout_s` | pending Episode apply |
| `0x05` | TMP background period, s | 4 | `temperature.sample_period_s` | RAM/live-loop field |
| `0x06` | MPU burst duration, ms | 2 | `mpu.burst_duration_ms` | pending MPU apply |
| `0x07` | Uplink/telemetry window period, s | 4 | `radio.uplink_period_s` | pending TelemetryWindow apply |
| `0x08` | Event processing enable | 1 | `event_processing.enable` | pending BMA + Episode apply |
| `0x09` | MPU event-trigger enable | 1 | `mpu.event_trigger_enable` | pending MPU apply |
| `0x0A` | RF TX power, dBm | 1 | `radio.tx_power_dbm` | pending Radio/LoRaWAN apply |
| `0x0B` | RF spreading factor | 1 | `radio.spreading_factor` | pending Radio/LoRaWAN apply |
| `0x0C` | RF bandwidth index | 1 | `radio.bandwidth_index` | pending Radio/LoRaWAN apply |
| `0x0D` | RF coding rate | 1 | `radio.coding_rate` | pending Radio/LoRaWAN apply |
| `0x0E` | RF TX timeout, ms | 2 | `radio.tx_timeout_ms` | pending Radio/LoRaWAN apply |
| `0x0F` | RF retry delay, ms | 2 | `radio.retry_delay_ms` | pending Radio/LoRaWAN apply |
| `0x10` | RF max TX attempts | 1 | `radio.max_tx_attempts` | pending Radio/LoRaWAN apply |

The existing `BolusRuntimeConfig_Validate()` remains the authoritative final range/consistency validator. For example, the present firmware only accepts the Step sensitivity values already permitted by RuntimeConfig validation; the downlink parser does not bypass that policy.

BMA Step sensitivity and BMA Event sensitivity remain separate commands and separate fields.

## Offline configurator

Folder:

```text
Bolus_Downlink_Configurator/
```

Open:

```text
Bolus_Downlink_Configurator/index.html
```

The page has no external dependency and is intended to run locally in a browser. It provides:

- per-setting checkboxes;
- grouped Motion/Event, Temperature/MPU, Telemetry and RF settings;
- transaction ID handling;
- little-endian TLV construction;
- space-separated HEX output;
- compact HEX output;
- Base64 output;
- FPort reminder;
- FPort-4 ACK/NACK decoder including apply-mask interpretation.

The tool is also marked **STAGING / UNTESTED**. It is a payload-construction helper, not evidence of end-to-end validation.

## ACK/NACK response packet

Fixed 8 bytes on FPort 4:

```text
byte 0    0xD2 response magic
byte 1    protocol version = 1
byte 2    transaction id
byte 3    result code
byte 4-5  apply mask, little-endian
byte 6-7  RuntimeConfig version, little-endian
```

Key result values:

- `0x00` accepted, but cached-service live apply still pending
- `0x01` accepted; no cached-service reconfiguration needed
- `0x02` duplicate successful transaction; not applied twice
- `0x80+` NACK/error conditions

## Debugger structures

Primary downlink structure:

`downlink_management_diag`

Important fields:

- `validation_state` — remains `DOWNLINK_VALIDATION_UNTESTED` in this checkpoint
- `rx_frame_count`
- `accepted_count`
- `duplicate_count`
- `rejected_count`
- `last_transaction_id`
- `last_command_count`
- `last_command_id`
- `last_result`
- `last_apply_mask`
- `pending_apply_mask`
- `ram_config_commit_count`
- `apply_complete_count`
- `apply_failure_count`

LoRaWAN-side observations remain in:

`lorawan_uplink_service_diag`

including:

- `downlink_count`
- `downlink_command_count`
- `downlink_wrong_port_count`
- `downlink_response_queued_count`
- `downlink_response_tx_success_count`
- `downlink_response_tx_failure_count`
- `downlink_response_drop_count`
- RSSI/SNR/RX slot metadata

## Explicitly NOT tested yet

- Clean CubeIDE compile/link after these changes.
- OTAA join with real credentials.
- FPort 3 command reception through a real gateway/network server.
- RX1 versus RX2 command reception.
- FPort 4 ACK/NACK transmission.
- Duty-cycle interaction with control-response priority.
- Duplicate transaction behavior over the network.
- Invalid/malformed payload rejection on target hardware.
- RuntimeConfig candidate validation on target hardware.
- Live application of BMA/EventEpisode/MPU/TelemetryWindow cached configuration.
- Live application of RF commands into LoRaMAC/RegionEU868/radio state.
- Browser configurator cross-check against a real network-server downlink.
- Persistence of modified config across reset/power loss.
- LoRaMAC NVM/session persistence.
- STOP2/RTC/LPTIM interaction.
- Current consumption.

## Required test order

1. Run the LoRaWAN CubeIDE enable script and do a clean Debug build.
2. Confirm existing Phase-4 sensor/radio bring-up regression still works.
3. With credentials still disabled, verify no Join Request is sent.
4. Provision real OTAA credentials privately and validate join.
5. Validate one normal Telemetry V2 uplink.
6. Send one valid FPort-3 downlink that only changes TMP background period.
7. Confirm atomic RAM config update and FPort-4 ACK.
8. Send malformed/unknown commands and confirm NACK with no partial config change.
9. Validate duplicate transaction behavior.
10. Cross-check one payload generated by `Bolus_Downlink_Configurator/index.html` against manual bytes.
11. Validate RF commands update RuntimeConfig and set `DOWNLINK_APPLY_RADIO_POLICY` without claiming live PHY change.
12. Implement and separately test cached-service and LoRaWAN RF live reconfiguration, then clear apply-mask bits only on actual success.
13. Only after those tests change the validation status from UNTESTED.
