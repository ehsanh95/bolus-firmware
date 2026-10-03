# Functional validation — 2026-10-03

Base: `origin/phase6/low-power-stop2`, `51f51b683ec58133215f5c2df8241dabfaf349f3`.
Working branch: `test/functional-downlink`. No hardware pass is claimed.

## Executed

`bash tests/run_host_tests.sh`: PASS, using host GCC and UndefinedBehaviorSanitizer.

- All 21 downlink command IDs: valid input, expected apply mask, duplicate handling, short/wrong lengths.
- Atomic rejection of a multi-command packet with a bad trailing command.
- Invalid magic/version/boolean/temperature/period, null input and insufficient response buffer.
- Failed apply keeps its pending bit; successful retry clears pending/failed bits.
- 20,000 deterministic random frames (lengths 0–255), rejection leaves RAM configuration unchanged.
- Motion at tick zero, guard boundary, tick wrap, quiet/max timeout, thermal-first/motion followups, six acquisition levels.
- Existing C telemetry encoder and JS V2/V2.1/V2.2/V3 decoder vectors.
- GCC syntax-only validation of 36 App/Core C translation units: PASS. This is NOT an ARM compile/link.
- CubeIDE configuration patch run twice: second run unchanged; both Debug/Release include App and selected LoRaWAN sources.

## Changes

1. Reject temperature limits outside the application's supported -55 to +150 C range before RAM commit.
2. Use pulse count rather than nonzero timestamp to recognize an existing motion pulse.
3. Schedule temperature followups from first motion even when a thermal alert started the episode.
4. Add missing ApplyRadioMib prototype and explicit stddef include.
5. Repair Debug/Release middleware/include source lists.
6. Add debugger diagnostics without UART output or changes to the wire protocol.

## Build and bench prerequisites

1. Import this checkout into STM32CubeIDE. Run `python tools/enable_lorawan_cubeide.py`; refresh, clean, build Debug and Release. Record full logs. No ARM toolchain is installed in the execution environment used for this report.
2. Provision your device locally. This snapshot has `BOLUS_LORAWAN_CREDENTIALS_PROVISIONED=0`: it cannot pass radio tests in WAIT_CREDENTIALS. Never put keys in test logs or screenshots.
3. Match the existing EU868 / ABP / Class A configuration to the test network. Runtime settings are RAM-only. MAC context persistence is disabled; after reset, do not assume the server will accept reset frame counters. Re-provision a fresh test session according to your server's procedure; do not disable replay protection as a fix.
4. Record firmware commit/build, device identity (no keys), gateway, server, initial configuration and reset cause.
5. Use running-debug/live expressions where possible. Halting on a breakpoint during TX/RX changes timings and can invalidate RX-window results.
6. Start with the existing radio settings. Change SF/BW/power only after basic downlink works. Duty-cycle deferrals are observable and are not automatically packet loss.

## Watch expressions

- `sensor_service_config`: accepted RAM values, not proof of hardware apply.
- `downlink_management_diag`: rx/accepted/rejected/duplicate counters, pending_apply_mask, failed_apply_mask, last_attempted_apply_mask.
- `last_payload_size`, `last_command_id`, `last_command_index` (zero-based, 255 before a TLV), `last_parse_offset` (start of last reached TLV; zero before parsing).
- `apply_deferred_radio_count`, `apply_deferred_episode_count`: counts of scheduler passes blocked, NOT elapsed time.
- `lorawan_uplink_service_diag`: state, credentials_provisioned, joined, TX success/failure, MAC status, duty-cycle defer, last_downlink_counter/port/size/slot/RSSI/SNR.
- New `rx_indication_count`, `rx_error_count`, `last_rx_status`, `rx_without_app_data_count`: distinguish MAC callback errors from application parser errors. Packets rejected internally by the MAC may not produce this callback.
- New `command_blocked_response_busy_count`: command discarded while previous response slot is occupied; resend with a fresh transaction after the previous response completes.
- `downlink_response_queued_count`, `downlink_response_tx_success_count`, `downlink_response_drop_count`: correlate with server receipt of FPort 4.
- `event_episode_service`, BMA/TMP IRQ counters, sensor status fields, `low_power_stop2_entry_count`.

## First downlink sequence

Send unconfirmed application downlinks on **FPort 3**, one at a time. Class A delivery follows an uplink. Wait for FPort 4 response before sending another. Hex vectors are in `tests/downlink_bench_vectors.json` (also Base64).

| Step | Payload hex | Expected |
|---|---|---|
| Set uplink period 60 s | D101010107043C000000 | response starts D20101001000; RAM period=60, pending bit 0x10 clears after apply |
| Repeat exact request | D101010107043C000000 | result 02, no new commit |
| Invalid bool | D1010201080102 | result 85, RAM unchanged |
| Atomic rollback | D101030207045A000000FF0100 | result 84; period stays 60 |
| Invalid 300 C high limit | D101040113023075 | result 86, RAM unchanged |
| Acquisition level 4 | D1010501110104 | result 00, mask 0x004D; all bits eventually clear when episode/radio idle |
| Restore acquisition level 3 | D1010601110103 | result 00, mask 0x004D |
| Restore default uplink 900 s | D1010701070484030000 | result 00, mask 0x0010 |

Response is 8 bytes: D2, protocol=1, transaction, result, apply-mask LE16, config-schema-version LE16. Version is NOT a commit counter. Result 00 means accepted/pending, not hardware success. Confirm `pending_apply_mask == 0` and actual sensor/radio behavior separately.

Duplicate suppression remembers only the most recently accepted transaction ID (not the entire payload). Reusing that ID with different values still yields duplicate; an older ID after an intervening transaction is accepted. Use fresh IDs for new commands.

## Full bench acceptance sheet — all NOT RUN

| ID | Exercise | Required evidence/pass condition |
|---|---|---|
| B01 | Cold boot, warm reset, Debug/Release | No HardFault; expected reset cause; sensors initialize; provisioned radio reaches idle |
| B02 | TMP periodic sampling and alert high/low/normal | Correct temperatures, IRQ source and episode source; clear alert does not loop |
| B03 | BMA motion, repeated pulses, quiet closure | One episode; guard suppresses close pulses; followups/quiet timeout correct |
| B04 | Thermal then motion, motion then thermal | Both source flags and temperature followups; max duration always closes |
| B05 | MPU acquisition levels 0–5 | Expected burst selection; sample count/status; no burst overlap with radio-critical section |
| B06 | Steps and telemetry window | Known step stimulus compared with window deltas, including first window and BMA reinitialization |
| B07 | Telemetry summary/continuation | Actual FPort 2 packets decode; sequence/order and digest coverage correct; no queue corruption |
| D01 | First downlink sequence above | Server payload + FPort 4 + before/after diagnostic snapshots |
| D02 | All remaining command IDs | Use existing configurator; RAM value, corresponding pending bit, hardware readback/behavior agree |
| D03 | Force RX1 and RX2 separately via server test setup | Gateway TX logs, callback slot and successful FPort 4 response for each |
| D04 | Wrong FPort, malformed/truncated, unknown command | Wrong-port count or NACK; no configuration commit |
| D05 | Apply while episode active / radio critical | Deferred counter increases; no premature sensor reset; apply completes once idle |
| D06 | Sensor apply fault and recovery | Failed bit remains, configuration not declared active; after recovery bits clear |
| D07 | Downlink during response backlog | Busy-drop diagnostic visible; previous ACK intact; subsequent resend works |
| D08 | RF policy changes | Apply result and MAC settings match; uplink/downlink remain reachable at new settings |
| R01 | No gateway / queue full / duty-cycle wait | Bounded queue/retry/drop, no lockup, resumes with gateway |
| R02 | TX timeout and recovery | Watchdog counters reflect recovery; late confirmation cannot pop unrelated packet |
| P01 | STOP2 + RTC, BMA, TMP and radio wake | No missed event, no premature sleep during RX1/RX2, time progression and watchdog correct |
| P02 | Repeated sleep/wake + long functional run | No HardFault/watchdog reset/queue growth or lost deadlines; record duration/event counts |
| P03 | Reset with queued data / ABP counters | Document data loss semantics and fresh-session procedure; production persistence remains unresolved |

## Open limitations / completion gate

- No actual board, debugger, gateway or network-server connection was available. All bench rows are NOT RUN. Do not call the entire functional qualification complete until they are recorded.
- Full ARM builds/link and actual RAM/flash usage remain unverified.
- No host MAC/queue integration simulation was added; parser tests do not prove LoRaWAN callbacks, response queue or live hardware apply.
- First BMA step sample establishes a baseline and reports zero; first-window steps before that sample are not measured. Needs bench policy/driver validation.
- STOP2 check-to-WFI race and queued-TX sleep policy remain for targeted bench investigation; no speculative power/radio timing changes were made here.
- Failed hardware apply is retried by the existing loop; investigate persistent failures and retry rate using new diagnostics.
- Runtime config and ABP counter persistence remain unresolved. Debug additions do not solve these.
