# Phase 5 — Event Episode Policy V2

This document records the current engineering contract for grouping BMA456 Any-Motion pulses, scheduling TMP117 measurements, and acquiring short MPU6050 bursts. It is a field-calibration implementation policy, not a validated cattle-event classifier.

> **STAGING / UNTESTED:** the Event Episode integration and the MPU6050 pulse-burst path have not yet been clean-built or verified on hardware because the board is temporarily unavailable. Do not mark these paths PASS until later bench evidence exists.

## Definitions

- **Motion pulse:** one accepted BMA456 Any-Motion indication after the short episode-level retrigger guard.
- **Event Episode:** one or more accepted motion pulses grouped together until a quiet interval closes the episode.
- **BMA Duration:** hardware Any-Motion persistence before INT1; not Episode duration.
- **Retrigger guard:** short software anti-chatter guard. Default: 2 s.
- **Episode quiet timeout:** silence after the latest accepted pulse before closing the episode. Default: 120 s.
- **MPU burst:** short, power-gated 6-axis observation performed once for every accepted pulse. Default: 250 ms at 100 Hz.

## RuntimeConfig Version 10 defaults

- Episode quiet timeout: 120 s
- Episode retrigger guard: 2 s
- First-pulse TMP follow-up offsets: +5 s, +15 s, +35 s, +65 s
- First pulse requests TMP immediately at t0
- Accepted pulse #2+ requests TMP immediately and cancels the remaining timer-driven TMP follow-up
- MPU event trigger: enabled
- MPU burst duration: 250 ms
- MPU sample rate: 100 Hz (~25 samples/burst)
- MPU accelerometer range: ±4 g
- MPU gyroscope range: ±500 dps

These are engineering defaults and remain calibration parameters.

## Episode state machine

1. The first accepted BMA pulse starts a new Event Episode.
2. The first pulse requests an immediate TMP117 one-shot measurement.
3. The first pulse also requests one short MPU6050 burst.
4. If no later pulse arrives, sparse TMP follow-up is scheduled at +5/+15/+35/+65 s from episode start.
5. Any accepted later pulse remains in the same Episode and resets the close deadline to `now + 120 s`.
6. Pulse #2 cancels all remaining timer-driven TMP follow-up for that Episode.
7. Pulse #2 and every later accepted pulse request one immediate TMP measurement and one short MPU burst.
8. Pulses inside the 2 s retrigger guard are rejected and do not trigger TMP or MPU acquisition.
9. The Episode closes only after 120 s without an accepted pulse.

## MPU energy/data policy

MPU6050 is normally physically power-gated OFF. For one accepted pulse the intended staged path is:

`Power ON -> configure/wake -> ~250 ms burst -> online feature reduction -> software sleep -> physical Power OFF`

Normal mode does not retain the raw 6-axis waveform. The burst is reduced into compact features:

- peak dynamic acceleration
- RMS dynamic acceleration
- peak angular velocity
- RMS angular velocity
- integrated total angular motion
- roll/pitch-derived orientation change when gravity-vector quality is acceptable

Physical rail-off is attempted on both success and failure paths.

## Temperature examples

### Single-pulse episode

- 0 s: pulse #1 + TMP + MPU burst
- 5 s: TMP follow-up
- 15 s: TMP follow-up
- 35 s: TMP follow-up
- 65 s: TMP follow-up
- 120 s: Episode closes if no new pulse occurred

### Multi-pulse episode

- 0 s: pulse #1 + TMP + MPU burst; TMP follow-up armed
- 5 s: scheduled TMP
- 43 s: pulse #2 + TMP + MPU burst; remaining scheduled TMP cancelled; close deadline = 163 s
- 91 s: pulse #3 + TMP + MPU burst; close deadline = 211 s
- 211 s: Episode closes if no later pulse occurred

## Low-power boundary

`EventEpisodeService` itself is non-blocking: it does not call `HAL_Delay`, read sensors, or enter low power. It receives an external monotonic time and emits actions.

Current `main.c` still supplies `HAL_GetTick()`. STOP2 and RTC/LPTIM migration is not implemented yet. Production integration must replace this timebase with one that continues while the Cortex-M4 is stopped.

The MPU burst acquisition is currently a bounded blocking service call (~250 ms plus setup) once an accepted pulse has already woken the MCU. This is a staging implementation; later power/timing characterization may justify FIFO/DMA or a more asynchronous burst path.

## Current integration boundary

Prepared in code, but **UNTESTED**:

- BMA INT1 -> Event Episode pulse handling
- immediate/scheduled TMP policy
- 2 s retrigger guard and 120 s quiet timeout
- MPU burst on every accepted pulse
- online MPU feature extraction and Episode aggregation
- 15-minute window aggregation and compact telemetry V2 preparation

Not yet connected:

- actual STOP2 entry + RTC/LPTIM wake scheduling
- EventProcessor biological/classifier outputs
- radio pending/retry queue
- actual LoRa/LoRaWAN transmission of the V2 payload
- board-level current characterization
