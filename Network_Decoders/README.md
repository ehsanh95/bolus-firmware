# Bolus Network-Server Uplink Decoders

Status: **DECODER IMPLEMENTED / OFFLINE VECTOR-TESTED / LORAWAN END-TO-END UNTESTED**

This folder contains ready-to-paste JavaScript uplink decoders for the current Bolus application wire protocol. Both decoders keep the frozen Telemetry V2 and V2.1 paths while using compact Telemetry V2.2 for new firmware uplinks.

## Files

- `the_things_stack_uplink_decoder.js` — The Things Stack / TTN JavaScript uplink payload formatter.
- `chirpstack_uplink_decoder.js` — ChirpStack v4 JavaScript codec.

Both files are intentionally standalone so they can be copied directly into the corresponding network-server UI without imports or build tooling.

## Decoded application ports

| FPort | Payload | Size |
|---:|---|---:|
| 2 | Telemetry V2 summary (legacy) | 32 bytes |
| 2 | Telemetry V2.1 summary with native BMA456 data | 42 bytes |
| 2 | Telemetry V2.2 compact summary (active) | 38 bytes |
| 4 | Downlink ACK/NACK response | 8 bytes |

FPort 3 is the Bolus configuration **downlink** port and is therefore not decoded as an uplink.

## Telemetry contract

The Telemetry V2 payload contract is frozen for decoder integration and is defined by `App/Services/telemetry_codec.c`. Its header remains `0x21`, and all 32 bytes retain their existing meaning.

Telemetry V2.1 uses header `0x31`. Bytes 0–31 preserve the complete V2 layout, while bytes 32–41 append:

| Bytes | Field | Encoding |
|---:|---|---|
| 32–35 | `bma.steps` | BMA456 Step Counter, unsigned 32-bit little-endian |
| 36–37 | `bma.accel_x_mg` | BMA456 X acceleration, signed 16-bit little-endian mg |
| 38–39 | `bma.accel_y_mg` | BMA456 Y acceleration, signed 16-bit little-endian mg |
| 40–41 | `bma.accel_z_mg` | BMA456 Z acceleration, signed 16-bit little-endian mg |

The decoder reports the application-facing version string as `"V2.1"`; wire version value `3` is retained under `protocol.version`.

Byte 31 is the event/reference flag byte, not an application CRC.

Candidate counters and event/reference flags are explicitly emitted as staging/research fields and must not be interpreted as validated physiological diagnoses.

Telemetry V2.2 uses header `0x41` and the following compact layout:

| Bytes | Field | Encoding |
|---:|---|---|
| 0 | protocol/message header | `0x41` |
| 1–2 | sequence | unsigned 16-bit little-endian |
| 3 | RuntimeConfig version | unsigned 8-bit |
| 4 | status/validity bitmap | unsigned 8-bit |
| 5–6 | battery voltage | unsigned 16-bit little-endian mV |
| 7–14 | current/min/max/negative-excursion temperatures | four signed 16-bit little-endian centi-°C values |
| 15–20 | Episode, pulse and inter-pulse fields | six unsigned 8-bit values |
| 21–27 | quantized MPU features | seven unsigned 8-bit values |
| 28–31 | `bma.steps` | unsigned 32-bit little-endian |
| 32–37 | BMA456 X/Y/Z acceleration | three signed 16-bit little-endian mg values |

V2.2 removes the on-air battery percentage, unconnected candidate counters and event/reference flags. Its decoded JSON exposes battery voltage only in millivolts and temperature only in degrees Celsius, avoiding duplicate representations of the same measurement.

## Validation boundary

The two JavaScript files have been syntax-checked and exercised locally with fixed Telemetry V2, Telemetry V2.1, Telemetry V2.2, and ACK/NACK vectors.

This is an **offline codec test**, not a LoRaWAN gateway/hardware PASS. OTAA, over-the-air uplink delivery, network-server execution, RX1/RX2 downlink delivery, and FPort-4 return traffic remain to be validated on the real system.
