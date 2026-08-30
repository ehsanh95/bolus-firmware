# Phase 5 — Event Data Pipeline and Telemetry V1

## Scope

This note freezes the current Phase-5 data contract before hardware orchestration is connected. It separates:

1. BMA Any-Motion trigger policy.
2. Short high-detail event capture.
3. Sparse temperature follow-up.
4. Local event feature storage and 15-minute aggregation.
5. Compact binary telemetry encoding.

The design intentionally sends information/features over LoRa rather than raw sensor waveforms in normal mode.

## Event timeline

An accepted BMA event starts a short motion-capture window.

- Motion capture target: approximately 2 s.
- BMA samples are reduced online into compact features; raw XYZ is not retained in normal mode.
- TMP117 is sampled at event time (`t0`).
- Temperature follow-up target points are `t0`, `+5 s`, `+10 s`, `+20 s`.
- The follow-up schedule is intentionally sparse so a delayed cooling response can be observed without keeping the MCU fully active for 20 s.
- The exact drinking-response latency is not considered biologically fixed; field data will tune these offsets.
- MPU6050 remains outside the V1 event path until the BMA/TMP path is proven. Later it can be enabled selectively for unusual rotational/high-energy events.

## Event feature record

`App/Application/event_data.h` defines `bolus_event_feature_record_t`.

Normal-mode motion features:

- event sequence and timestamp
- capture duration and sample count
- peak dynamic acceleration
- RMS dynamic acceleration
- dynamic-acceleration variance
- peak jerk
- RMS jerk
- inter-event interval when available
- sparse event-related temperature points
- optional later rotation fields
- processor/reference flags

Dynamic acceleration is derived from the orientation-independent acceleration magnitude relative to 1 g. Jerk is derived from the resultant change in XYZ acceleration divided by the measured sample interval.

The event extractor (`EventFeatureExtractor`) updates sums/peaks online and therefore does not allocate a raw 2-s waveform buffer.

## Physiological timing separation

Do not confuse three different timescales:

- BMA Any-Motion `Duration`: 200–800 ms in the current bundled profiles; this is the trigger persistence requirement.
- short detailed event capture: approximately 2 s; this supplies compact physical-motion features.
- contraction morphology: approximately 8–10 s with roughly 40–60 s recurrence; this belongs to the longer BMA observation/pattern layer and must not be inferred solely from the 2-s capture duration.

## BMA event profiles

RuntimeConfig version 8 resolves the first in-animal engineering sweep as:

| Profile | Threshold | Duration | Cooldown |
|---|---:|---:|---:|
| VERY_LOW | 900 mg | 800 ms | 60 s |
| LOW | 750 mg | 600 ms | 45 s |
| LEVEL_1 | 600 mg | 500 ms | 30 s |
| LEVEL_2 | 500 mg | 400 ms | 15 s |
| LEVEL_3 | 400 mg | 300 ms | 5 s |
| LEVEL_4 | 300 mg | 200 ms | 0 s |
| RAW | direct config | direct config | direct config |
| OFF | event disabled | — | — |

LEVEL_2 is the current first in-animal development default. These values are calibration candidates, not validated cattle thresholds. Step Counter sensitivity remains independent.

## 15-minute telemetry snapshot

`bolus_telemetry_summary_v1_t` is the internal frozen summary model. Before transmission the production scheduler will perform a final acquisition (TMP117, BMA summary, battery, fault state), update the current aggregation window, freeze the snapshot, then encode it.

The active collection window must be separated from the pending-TX snapshot so sensor collection can continue while a previous packet is retried.

## Compact Summary V1 wire format

The V1 summary encoder produces exactly 24 bytes. Multibyte fields are little-endian.

| Byte(s) | Field |
|---|---|
| 0 | protocol version nibble + message type nibble |
| 1–2 | uplink sequence |
| 3 | config version |
| 4 | validity/health/status bitmap |
| 5 | battery percent |
| 6–7 | battery mV |
| 8–9 | current temperature, centi-C |
| 10–11 | minimum temperature, centi-C |
| 12–13 | maximum temperature, centi-C |
| 14–15 | maximum negative temperature excursion, centi-C signed |
| 16 | motion-event count, saturated |
| 17 | contraction-candidate count, saturated |
| 18 | rotation-candidate count, saturated |
| 19 | mean inter-event interval in seconds, saturated |
| 20 | inter-event interval standard deviation in seconds, saturated |
| 21 | RMS dynamic acceleration quantized in 20-mg units |
| 22 | peak dynamic acceleration quantized in 20-mg units |
| 23 | low 8 bits of combined event/reference flags |

The 24-byte format is a protocol contract, not a C struct layout. `TelemetryCodec_EncodeSummaryV1()` serializes every field explicitly.

Detailed event records and raw diagnostic windows are separate future message types; they are not included in every normal uplink.

## Current implementation boundary

Implemented now:

- RuntimeConfig v8 BMA event sweep.
- application-level per-event data contract.
- online short-window BMA feature extractor.
- application-level 15-minute telemetry snapshot model.
- explicit fixed 24-byte Summary V1 encoder.

Still to wire/bench-prove:

- accepted BMA INT1 -> start event capture.
- t0 TMP117 acquisition.
- non-blocking 2-s BMA sample scheduling.
- sparse +5/+10/+20 s temperature follow-up scheduler.
- event ring/aggregator integration with the new BMA features.
- final scheduled acquisition and snapshot freeze.
- RadioService/LoRaWAN transmission of the encoded payload.

Do not mark these unwired items PASS until code, build and hardware evidence exist.
