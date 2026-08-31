# Phase 5 Hardware Validation Test #1

Date: 2026-08-31

## Scope

First hardware validation of the Event-driven sensing pipeline after the Phase 5 staging implementation.

Tested chain:

`BMA456 INT1 -> Event Episode -> TMP117 acquisition -> MPU6050 burst -> Telemetry V2 preparation`

## Test environment

- Prototype Bolus board available
- Firmware branch: `phase5/application-architecture`
- RuntimeConfig version: 10
- Debug validation through Live Expressions

## Initialization Results

| Component | Result |
|---|---|
| RuntimeConfig | PASS |
| BMA456 service | PASS |
| BMA Event service | PASS |
| TMP117 service | PASS |
| MPU6050 service | PASS |
| Event Episode service | PASS |
| Telemetry Window service | PASS |
| RFM95W regression status | PASS |

## Event Episode Validation

### Test 1 — Multiple pulses inside one episode

Result: PASS

Observed behavior:

- Multiple accepted motion pulses were grouped into the same Event Episode.
- Episode close counter did not increment during active episode continuation.
- Motion pulse count increased correctly.

### Test 2 — Retrigger guard validation

Result: PASS

Procedure:

- Generated closely spaced motion pulses below the episode retrigger guard window.

Observed:

- `event_episode_retrigger_suppressed_count` increased.
- Accepted motion pulse count did not increase for suppressed retriggers.
- TMP117 and MPU6050 acquisition were not triggered for suppressed pulses.

This confirms separation between:

1. BMA interrupt generation
2. Event Episode acceptance policy

## MPU6050 Burst Validation

Observed:

- MPU burst counter increased with accepted motion pulses.
- Last MPU sample count observed: approximately 25 samples.

Expected configuration:

- Burst duration: 250 ms
- Sampling rate: 100 Hz
- Target samples: ~25

## Telemetry V2 Validation

Observed:

- `telemetry_payload_v2_ready = true`
- `telemetry_payload_v2_size = 32`
- Telemetry snapshot counter increased.

Result:

Telemetry preparation pipeline is operational.

## Remaining Validation Items

Not covered by this test:

- MPU feature numerical quality (RMS/peak/gyro/orientation values)
- Long 15-minute no-event window validation
- Actual LoRa transmission
- STOP2 current measurement
- Battery lifetime characterization
- Downlink configuration management

## Conclusion

Phase 5 event-driven sensing architecture passed the first hardware validation milestone.

The following modules have demonstrated functional integration on prototype hardware:

- BMA456 event detection
- Event Episode state handling
- TMP117 event sampling policy
- MPU6050 burst triggering
- Telemetry V2 payload generation

Further work should continue with feature validation, power characterization, and RF/system integration.