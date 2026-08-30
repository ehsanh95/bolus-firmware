# Phase 5 — Event Episode Data Pipeline and Telemetry V2

## Status

> **UNTESTED STAGING — 2026-08-30**
>
> The Event Episode, pulse-driven MPU6050 burst, 15-minute aggregation, and Telemetry V2 preparation paths were implemented while the hardware board was unavailable. They have not yet been clean-built, flashed, or validated on the board. No Phase-5 PASS claim is made for these paths.

## Scope

The current pipeline separates four contracts:

1. BMA456 Any-Motion pulse detection.
2. Event Episode lifecycle and sparse temperature policy.
3. Short per-pulse MPU6050 feature bursts.
4. 15-minute aggregation and explicit compact telemetry encoding.

Normal telemetry sends compact information/features, not raw sensor waveforms.

## Pulse and Episode timeline

### Accepted first pulse

- BMA pulse passes the Episode retrigger guard.
- Start a new Episode.
- Take TMP117 immediately.
- Power on MPU6050 and acquire one short burst.
- Arm TMP follow-up at +5/+15/+35/+65 s.
- Set Episode close deadline to `pulse_time + 120 s`.

### Accepted pulse #2+

- Keep the same Episode.
- Record inter-pulse interval.
- Take TMP117 immediately.
- Acquire another short MPU burst.
- Cancel all remaining timer-driven TMP follow-up for the Episode.
- Reset close deadline to `pulse_time + 120 s`.

### Rejected chatter pulse

A BMA indication inside the 2 s Episode retrigger guard is suppressed. It does not trigger TMP or MPU acquisition and does not advance the accepted-pulse sequence.

### Episode close

An Episode closes after 120 s without an accepted motion pulse.

The 2 s guard and 120 s quiet timeout are engineering parameters, not validated physiological thresholds.

## BMA Any-Motion policy

RuntimeConfig Version 10 keeps BMA Step Counter sensitivity independent from Any-Motion sensitivity.

Bundled event profiles use zero long service-level cooldown so the Episode layer can observe pulse timing:

| Profile | Any-Motion threshold | Duration | Bundled cooldown |
|---|---:|---:|---:|
| VERY_LOW | 900 mg | 800 ms | 0 s |
| LOW | 750 mg | 600 ms | 0 s |
| LEVEL_1 | 600 mg | 500 ms | 0 s |
| LEVEL_2 | 500 mg | 400 ms | 0 s |
| LEVEL_3 | 400 mg | 300 ms | 0 s |
| LEVEL_4 | 300 mg | 200 ms | 0 s |
| RAW | direct config | direct config | direct config |
| OFF | disabled | — | — |

LEVEL_2 remains the development default. These are engineering calibration candidates, not cattle thresholds.

## MPU6050 pulse burst

Version 10 default:

- event-trigger enabled
- duration: 250 ms
- sample rate: 100 Hz (~25 target samples)
- accel range: ±4 g
- gyro range: ±500 dps

For every accepted pulse:

`rail ON -> configure -> wake/stabilize -> burst samples -> online reduction -> sleep -> rail OFF`

Raw burst samples are discarded in normal mode. Compact features are retained:

- peak/RMS dynamic acceleration
- peak/RMS angular velocity
- total angular motion
- roll change / pitch change internally
- orientation-change magnitude when valid

The service explicitly powers the MPU rail OFF on success/failure cleanup paths.

## Temperature policy

TMP117 remains one-shot/shutdown between readings.

Single-pulse Episode:

`0 s, 5 s, 15 s, 35 s, 65 s`

Multi-pulse Episode:

- pulse #1 starts the sparse schedule
- pulse #2 cancels remaining scheduled follow-up
- pulse #2+ supplies immediate temperature observations at the natural motion-pulse times

A separate sparse background TMP baseline remains configured at 600 s by default.

## 15-minute window aggregation

`TelemetryWindowService` collects across the active 15-minute window:

- Episode count
- accepted pulse count
- retrigger-suppressed pulse count
- maximum pulses observed in one Episode
- inter-pulse interval mean/variance
- temperature current/min/max/max-negative-excursion
- successful MPU burst count
- average MPU RMS acceleration and angular velocity
- MPU peak acceleration and angular velocity
- total angular motion
- maximum orientation change
- classifier placeholders / event flags

At the 15-minute boundary the current staging path performs final TMP, final BMA sample, battery measurement, and fault/health capture, then freezes a summary and immediately rolls the active accumulator into the next window.

## Telemetry V2 wire format

Telemetry V2 is an explicit **32-byte** binary packet. Multibyte fields are little-endian.

| Byte(s) | Field |
|---|---|
| 0 | protocol version 2 + summary message type |
| 1–2 | sequence |
| 3 | RuntimeConfig version |
| 4 | validity / health / `STAGING_UNTESTED` bitmap |
| 5 | battery percent |
| 6–7 | battery mV |
| 8–9 | current temperature, centi-°C |
| 10–11 | minimum temperature, centi-°C |
| 12–13 | maximum temperature, centi-°C |
| 14–15 | maximum negative temperature excursion, signed centi-°C |
| 16 | Episode count |
| 17 | accepted pulse count |
| 18 | guard-suppressed pulse count |
| 19 | max pulses in one Episode |
| 20 | mean inter-pulse interval, seconds |
| 21 | inter-pulse interval standard deviation, seconds |
| 22 | successful MPU burst count |
| 23 | mean MPU dynamic RMS / 20 mg |
| 24 | MPU peak dynamic acceleration / 20 mg |
| 25 | mean MPU angular-velocity RMS / 10 dps |
| 26 | MPU peak angular velocity / 10 dps |
| 27 | max orientation change / 2 degrees |
| 28 | total angular motion / 5 degrees |
| 29 | contraction-candidate count — reserved until classifier connected |
| 30 | rotation-candidate count — reserved until classifier connected |
| 31 | low 8 bits of combined event/reference flags |

Legacy 24-byte Telemetry V1 remains in the codec for compatibility, but the new Episode architecture prepares V2.

## Current implementation boundary

### Prepared in code but not validated

- RuntimeConfig V10
- Event Episode grouping
- pulse-driven/scheduled TMP policy
- short power-gated MPU burst per accepted pulse
- online MPU feature extraction
- Episode MPU aggregation
- 15-minute telemetry window aggregation
- final scheduled measurement snapshot
- explicit 32-byte Telemetry V2 encoding
- `telemetry_payload_v2_ready` staging flag for debugger inspection

### Deliberately not connected yet

- actual LoRa/LoRaWAN TX of V2
- pending-TX/retry queue and durable frozen packet ownership
- STOP2 + RTC/LPTIM scheduler migration
- classifier-generated contraction/rotation candidate counts
- full board/build/hardware validation
- measured power characterization

The current implementation stops at **telemetry preparation**, as intended for this staging pass.
