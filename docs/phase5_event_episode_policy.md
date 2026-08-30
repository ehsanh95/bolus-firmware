# Phase 5 — Event Episode Policy V1

This document freezes the current engineering contract for grouping BMA456 Any-Motion pulses and scheduling TMP117 measurements. It is an implementation policy for field calibration, not a validated cattle-event classifier.

## Definitions

- **Motion pulse**: one accepted BMA456 Any-Motion indication after the short episode-level retrigger guard.
- **Event Episode**: one or more accepted motion pulses grouped together until a quiet interval closes the episode.
- **BMA Duration**: the hardware Any-Motion persistence requirement before INT1. It is not Event Episode duration.
- **Retrigger guard**: a short software guard against overlapping/chatter pulses. Default: 2 s.
- **Episode quiet timeout**: silence required after the most recent accepted pulse before closing the episode. Default: 120 s.

## Default timing

RuntimeConfig Version 9 defaults:

- Episode quiet timeout: 120 s
- Episode retrigger guard: 2 s
- First-pulse temperature follow-up offsets: +5 s, +15 s, +35 s, +65 s
- First pulse always requests TMP immediately at t0

All values are field-calibration parameters and must remain downlink-ready.

## Episode state machine

1. The first accepted BMA pulse starts a new Event Episode.
2. The first pulse requests an immediate TMP117 one-shot measurement.
3. If no later pulse arrives, sparse TMP follow-up is scheduled at +5/+15/+35/+65 s from episode start.
4. Any accepted later pulse remains inside the same Event Episode and resets the episode close deadline to `now + 120 s`.
5. Pulse #2 cancels all remaining timer-driven TMP follow-up for that episode.
6. Pulse #2 and every later accepted pulse request one immediate TMP measurement instead.
7. The episode closes only after 120 s without an accepted pulse.

This gives a single-pulse episode a useful temperature trajectory without forcing repeated RTC wakeups when BMA pulses already provide natural observation times.

## Examples

### Single-pulse episode

- 0 s: pulse #1 + TMP
- 5 s: TMP follow-up
- 15 s: TMP follow-up
- 35 s: TMP follow-up
- 65 s: TMP follow-up
- 120 s: episode closes if no new pulse occurred

### Multi-pulse episode

- 0 s: pulse #1 + TMP; follow-up schedule armed
- 5 s: scheduled TMP
- 43 s: pulse #2 + TMP; remaining scheduled follow-up cancelled; close deadline becomes 163 s
- 91 s: pulse #3 + TMP; close deadline becomes 211 s
- 211 s: episode closes if no later pulse occurred

## Energy behavior

The scheduler is non-blocking. `EventEpisodeService` never calls `HAL_Delay`, never waits for a deadline, never reads a sensor, and never enters/exits low power. It only receives an external monotonic timestamp and emits actions such as `take_temperature_now` or `episode_closed`.

Current bench integration supplies `HAL_GetTick()` because STOP2/RTC migration is not complete. Production STOP2 integration must replace that caller timebase with an RTC/LPTIM-backed monotonic time source that continues while the Cortex-M4 core is stopped.

Bundled BMA sensitivity profiles now resolve service-level cooldown to 0 s so the episode layer can observe the pulse sequence. RAW mode retains direct cooldown for bench experiments. The 2 s episode retrigger guard is the normal anti-chatter mechanism.

## Current integration boundary

As of this policy commit:

- BMA INT1 remains ISR-counter-only.
- BmaEventService acknowledges Bosch feature status in main context.
- EventEpisodeService groups accepted pulses and drives immediate/scheduled TMP requests.
- Background TMP sampling follows RuntimeConfig rather than the old 2 s bench loop.
- MPU6050 remains OFF after one startup regression acquisition; repeated 5 s bench reads are removed.
- Actual STOP2 entry and RTC alarm scheduling are a later Power/Low-Power integration step.
- EventFeatureExtractor, EventAggregator and telemetry codec remain separate layers and are not yet fully wired into the episode lifecycle.
