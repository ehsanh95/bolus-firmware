# Phase 6 — Adaptive Dual-Sentinel Acquisition

## Purpose

This revision changes Bolus from a fixed 15-minute aggregate acquisition path
to an adaptive, event-preserving low-power pipeline.

The two always-available sentinels are:

- **BMA456** — Step Counter + Any-Motion.
- **TMP117** — slow continuous conversion + independent HIGH/LOW Alert Mode.

The STM32 remains in STOP2 whenever cooperative work is drained. MPU6050
remains physically power-gated and is only enabled for selected informative
motion pulses.

## Acquisition levels

| Level | Motion | TMP117 | MPU policy |
|---|---|---|---|
| 0 | Any-Motion off | Shutdown, alert off | Off |
| 1 | 900 mg / 800 ms | Alert, 16 s conversion | First pulse |
| 2 | 750 mg / 600 ms | Alert, 16 s conversion | First + every 4th |
| 3 | 500 mg / 400 ms | Alert, 8 s conversion | First + every 3rd |
| 4 | 400 mg / 300 ms | Alert, 4 s conversion | First + every 2nd |
| 5 | 300 mg / 200 ms | Alert, 1 s conversion | Every accepted pulse |

Level 3 is the default. Levels change detection/detail, not Episode grouping
timeouts, so higher observation does not artificially split one physical event
into multiple Episodes.

At Level 0 there is no background MCU temperature read. TMP117 is in shutdown,
BMA Any-Motion is disabled, MPU is off, and the fresh sensor snapshot is taken
at the telemetry boundary.

## Thermal sentinel

TMP117 uses **Alert Mode**, not Therm Mode, because Bolus needs independent
HIGH and LOW abnormal-temperature detection.

The ALERT output is routed to **PC13 / EXTI13**. The ISR path only increments a
counter. I2C traffic happens in main context.

To avoid an interrupt storm while temperature remains outside the configured
window:

1. first HIGH/LOW alert wakes the STM32;
2. the firmware reads/acknowledges the TMP117 alert and starts/updates a thermal
   Episode;
3. EXTI13 is masked;
4. RTC-based follow-up reads are used while abnormal;
5. when temperature returns to the normal window, the thermal Episode closes
   and EXTI13 is re-armed.

The absolute high/low limits are calibration settings and are not automatically
made more aggressive by the Acquisition Level.

## Episode model

Episodes may be:

- Motion
- Thermal high
- Thermal low
- Combined motion + thermal

Every Episode has a maximum duration in addition to the existing quiet timeout.
Motion retrigger guard and quiet timeout retain their semantic meaning across all
Acquisition Levels.

Closed Episodes are converted to compact Event Digests and stored in an
16-record RAM ring buffer. A digest retains timing, pulse structure, temperature
change, selected motion features, source type and data-quality flags.

## Telemetry V3

V2, V2.1 and V2.2 codecs remain frozen and decoder-compatible. The active
firmware uplink is V3.

### Summary packet — message 0x51

18-byte header plus up to three 11-byte Episode Digests; maximum 51 bytes.

The header contains:

- sequence and RuntimeConfig version;
- status/health flags;
- Acquisition Level plus a custom-profile bit when advanced overrides are active;
- Event-Digest overflow status so the backend knows if any Episode detail was dropped;
- battery mV;
- current/min/max temperature;
- BMA step delta for this telemetry window;
- closed Episode count;
- suppressed motion-trigger count.

### Continuation packet — message 0x52

5-byte continuation header plus up to four Episode Digests. Continuations use
the same window sequence so the backend can reassemble them deterministically.

This means a normal window with 0–3 closed Episodes uses one application
uplink. Additional Episodes require continuation packets only when necessary.

## Downlink additions

FPort 3 keeps protocol version 1 and the TLV layout.

| Command | ID | Value |
|---|---:|---|
| Acquisition Level | 0x11 | u8, 0..5 |
| TMP alert enable | 0x12 | u8, 0/1 |
| TMP high limit | 0x13 | signed LE int16, centi-C |
| TMP low limit | 0x14 | signed LE int16, centi-C |
| TMP conversion cycle | 0x15 | u8, 0..7 |

The existing advanced commands remain available. Accepted configuration is
applied by the main scheduler only outside radio-critical windows.

## Radio policy

The previous hardcoded DR0 request was removed. The active EU868 request data
rate is derived from RuntimeConfig for valid LoRaWAN combinations. The default
SF7/BW125 profile maps to DR5. TX power is also applied through the LoRaMAC MIB.

The accepted live policy is intentionally constrained to combinations represented
by EU868 LoRaWAN data rates: BW125 with SF7..SF12, or BW250 with SF7/DR6;
coding-rate configuration is fixed to the regional LoRaWAN profile (4/5).
TX power accepts even values from 2 through 16 dBm so RuntimeConfig does not
claim a value that the regional power-index mapping cannot represent exactly.

Before a queued application packet is submitted, the LoRaMAC payload budget is
queried with `LoRaMacQueryTxPossible()`. If pending MAC commands consume FOpts
space, a MAC-only frame is sent first without popping the application queue, and
the same V3 packet is retried afterward.

## Validation status

The source contract, network decoders, offline decoder vectors and payload tool
are updated together in this revision. A clean STM32CubeIDE build plus
board/gateway validation is still required before production sign-off.


## Backend interpretation safeguards

- A V3 acquisition-profile byte carries level 0..5 in the low bits and marks
  `custom_profile` when an advanced acquisition downlink overrides the preset.
- V3 status marks `event_digest_overflow` if the 16-record RAM buffer overflowed.
  The server must treat that window as incomplete for Episode-level analysis.
- Step activity is transmitted as a per-window delta, not only as the cumulative
  native BMA456 counter.
