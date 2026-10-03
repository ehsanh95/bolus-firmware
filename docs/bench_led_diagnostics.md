# Bench LED and fault diagnostics

Scope: functional-validation bench firmware only. This is **not** an RF or
STOP2 current-consumption certification. The current on-board behavior still
requires clean STM32CubeIDE build and physical bench verification.

## Build switch

`App/BSP/bolus_led.h` controls `BOLUS_BENCH_LED_DIAGNOSTICS`:

- Existing STM32CubeIDE **Debug** (`DEBUG` defined): **1**, bench LEDs enabled.
- Existing STM32CubeIDE **Release** (`DEBUG` absent): **0**, LED GPIO writes
  are compiled out. The initial GPIO setup still holds all LED pins low.
- For current measurements using Debug, add
  `BOLUS_BENCH_LED_DIAGNOSTICS=0` to **all Debug C compiler** defined symbols,
  clean-build, and flash that binary. Debug should then behave like Release
  with respect to diagnostic LED GPIO writes.

Do not measure product current with the LED-enabled Debug firmware. Diagnostic
pulses can intentionally postpone STOP2 for at most 620 ms; this does not
change RF RX1/RX2 timings or add a separate wake timer.

## LED meaning (only when diagnostic build is enabled)

| LED | Pattern | Meaning |
| --- | --- | --- |
| LD1: sensor | One short pulse (120 ms) | Telemetry summary successfully frozen; **not** proof of LoRa transmission |
| LD1: sensor | Lit when CPU is awake | Active sensor or noncritical battery fault |
| LD2: MCU | Lit when CPU is awake | Active power/config/system/critical battery fault |
| LD2: MCU | Permanently lit | `Error_Handler` fatal loop on a Debug build |
| LD3: LoRa | One short pulse | MAC transmission request observed or telemetry waiting in the queue |
| LD3: LoRa | Two short pulses | Telemetry TX-success confirmation or FPort 4 response TX success |
| LD3: LoRa | One long pulse (~420 ms) | MAC duty-cycle defer; **not** automatically a hardware failure |
| LD3: LoRa | Two short pulses repeated on normal wake | Missing ABP session or MAC Busy event; use Watch data to distinguish |
| LD3: LoRa | Three short pulses | TX failure/drop; inspect `fault_manager_diag` and LoRa counters |
| LD3: LoRa | Lit when CPU is awake | Active RF fault |

**Important:** The LEDs are deliberately **off** while LoRaMAC is in a
radio-critical TX/RX interval and throughout STOP2. This prevents LED GPIO
activity from contaminating RX1/RX2 and prevents continuous LED load during
sleep. Therefore a brief pattern may be missed by eye; use the RAM counters
as the primary source of evidence. Persistent faults are redrawn on ordinary
wakeups; no special periodic RTC wake was added for LEDs.

A telemetry interval is **not** an RF success interval. After downlink changes
the uplink interval, LD1 can pulse every minute even if the LoRaWAN queue has
not made it through MAC admission, duty-cycle waiting, the TX-confirm callback
or gateway delivery.

## Debugger Watch checklist

Add `fault_manager_diag` and `bench_led_diag` in the STM32CubeIDE Expressions
view. `fault_manager_diag` is kept even with diagnostic LEDs disabled; it is
RAM-only and resets at reboot. High-value fields:

- `fault_manager_diag.active_mask`: faults currently asserted
- `fault_manager_diag.history_mask`: sticky mask of faults raised this boot
- `fault_manager_diag.last_raised_id`, `last_cleared_id`: distinguish a
  one-minute intermittent battery/sensor fault from a permanent failure
- `fault_manager_diag.raises_by_id`: per-fault counters by enum ID
- `bench_led_diag.sensor_snapshot_count`: successful telemetry freeze count
- `bench_led_diag.last_rf_event`: last selected visual RF indicator
- `bench_led_diag.tx_request_count` and `tx_success_count`: see whether
  telemetry reached MAC request and confirmed TX, respectively
- `bench_led_diag.tx_failure_count`, `tx_drop_count`: retry/failure evidence
- `bench_led_diag.duty_cycle_defer_count`, `mac_busy_defer_count`: distinguish
  radio scheduler deferral from application-side blockage
- `bench_led_diag.response_tx_success_count`: successful FPort 4 responses

Cross-check against the authoritative live counters in
`lorawan_uplink_service_diag` (`queue_count`, `tx_in_flight`,
`last_mac_status`, `last_mcps_confirm_status`, `last_sequence_submitted`,
`last_sequence_completed`, `last_uplink_counter`, `joined`,
`credentials_provisioned`). A MAC-confirmed TX is not proof of gateway receipt.
Compare `fCnt` and FPort 2 events in ChirpStack.

When a board previously sent every 60 seconds but stopped arriving:

1. Preserve current RAM diagnostics before flashing or rebooting. Reboots erase
   the RAM fault history and require normal ABP session/frame-counter handling.
2. Inspect sticky fault masks and compare LoRa request/success/drop and deferral
   counts; also check `telemetry_snapshot_failure_count`,
   `telemetry_payload_v3_ready`, and `telemetry_backpressure_defer_count`.
3. If the MAC reports success but ChirpStack receives nothing, check gateway
   reception and session/frame-counter rejection before changing MAC code.
4. Do not halt on a breakpoint during TX, RX1 or RX2.

## Implementation boundaries

- FaultManager owns fault IDs, sticky history and active fault semantics.
- `BenchLedDiagnostics_Process` is called only from the application main loop,
  after the LoRaWAN service has been processed. No delays or LED work are
  added to its MAC interrupts.
- `BenchLedDiagnostics_BeforeStop2` forces LEDs off before WFI.
- Release compiles out the renderer call sites and all BolusLed GPIO writes.
- Tests: `bash tests/run_host_tests.sh` includes Debug and Release build
  tests for the LED renderer. A real STM32CubeIDE build and bench check remain
  separate release gates.
