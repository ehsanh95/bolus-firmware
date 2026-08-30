# Phase 5 — UNTESTED Staging Checkpoint

Date: 2026-08-30
Branch: `phase5/application-architecture`

## Why this checkpoint exists

The physical Bolus board is temporarily unavailable. The firmware was intentionally advanced through Event Episode integration, event-driven MPU6050 acquisition, 15-minute aggregation, and telemetry-payload preparation without pretending that these stages were bench validated.

**Status: code prepared; build/hardware validation pending.**

No item in this document should be marked Phase-5 PASS solely because the source code exists.

## Staged but unvalidated chain

`BMA INT1 -> accepted motion pulse -> Event Episode -> TMP policy -> MPU burst -> local features -> 15-minute window -> final acquisition -> Telemetry V2 bytes`

Prepared behavior:

- BMA456 remains the always-on sentinel.
- Bundled BMA profiles use zero long cooldown; Episode layer uses a 2 s retrigger guard.
- First accepted pulse starts an Episode and arms 120 s quiet-close logic.
- First pulse takes TMP immediately and arms +5/+15/+35/+65 s follow-up.
- Pulse #2+ takes TMP immediately and cancels remaining timer-driven follow-up.
- Every accepted pulse requests one default 250 ms / 100 Hz MPU6050 burst.
- MPU6050 is power-gated OFF outside the burst.
- Raw MPU burst data is reduced online into compact features.
- 15-minute aggregation collects Episode, temperature, timing, and MPU statistics.
- At the 15-minute boundary final TMP/BMA/battery/fault data are acquired and a 32-byte Telemetry V2 payload is prepared.
- Actual RF transmission is not connected yet.

## Explicitly untested

The following have **not** been clean-built/flashed/verified after the recent staging changes:

1. RuntimeConfig V10 acceptance on the target build.
2. BMA IRQ -> Episode accepted/suppressed pulse behavior.
3. First-pulse TMP schedule and pulse #2 cancellation behavior.
4. 120 s quiet-close behavior.
5. MPU6050 power ON/OFF per accepted pulse.
6. MPU burst sample count/timing and failure cleanup.
7. MPU feature scaling and orientation-change validity.
8. Episode/window feature aggregation.
9. 15-minute final acquisition and Telemetry V2 freeze.
10. Exact 32-byte payload contents.
11. Watchdog coexistence during MPU bursts.
12. STOP2/RTC behavior — not implemented yet.
13. Radio pending/retry/TX — not implemented yet.
14. Real current consumption.

## Later verification order

When the board is available, validate in this order so failures can be isolated quickly:

1. Clean build only; resolve compile/link issues without changing behavior.
2. Boot/regression checks from Phase 4.
3. BMA interrupt path and 2 s retrigger guard.
4. Event Episode start/continue/120 s close.
5. TMP first-pulse schedule and pulse-driven cancellation.
6. MPU rail ON/OFF around one accepted pulse.
7. MPU default burst: 250 ms at 100 Hz, approximately 25 samples.
8. Repeated accepted pulses and MPU/TMP failure cleanup.
9. 15-minute telemetry-window aggregation.
10. Final TMP/BMA/battery/fault snapshot and 32-byte V2 encoding.
11. Add/test pending-TX ownership, retry behavior, and actual RF TX.
12. Migrate scheduling/timebase to RTC/LPTIM + STOP2.
13. Measure board current and tune the power budget.

## Rollback policy

The staging work was intentionally split into small commits. If a later test fails, inspect or revert one commit/layer at a time instead of rewriting the whole architecture.

The most important rollback boundaries are:

- MPU driver burst primitives
- RuntimeConfig V10
- SensorService MPU burst feature extraction
- EventEpisode MPU integration
- main-loop pulse/MPU wiring
- Telemetry window aggregation
- Telemetry V2 data contract/codec
- 15-minute telemetry preparation in `main.c`

## Known staging limitations

- `HAL_GetTick()` is still the current time source. It is not the final STOP2-safe timebase.
- MPU burst acquisition is currently bounded but blocking; BMA ISR edges can still be counted while it runs, but exact high-density pulse timing is not yet proven.
- The V2 packet is frozen/prepared but not transmitted.
- Pending-TX retry ownership is not implemented; the debugger staging payload can be replaced by the next 15-minute freeze.
- Classifier bytes for contraction/rotation candidates are reserved and remain zero until EventProcessor integration.
- Engineering thresholds/timings are calibration candidates, not validated cattle diagnostic thresholds.
