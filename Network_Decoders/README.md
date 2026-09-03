# Bolus Network-Server Uplink Decoders

Status: **DECODER IMPLEMENTED / OFFLINE VECTOR-TESTED / LORAWAN END-TO-END UNTESTED**

This folder contains ready-to-paste JavaScript uplink decoders for the current Bolus application wire protocol.

## Files

- `the_things_stack_uplink_decoder.js` — The Things Stack / TTN JavaScript uplink payload formatter.
- `chirpstack_uplink_decoder.js` — ChirpStack v4 JavaScript codec.

Both files are intentionally standalone so they can be copied directly into the corresponding network-server UI without imports or build tooling.

## Decoded application ports

| FPort | Payload | Size |
|---:|---|---:|
| 2 | Telemetry V2 summary | 32 bytes |
| 4 | Downlink ACK/NACK response | 8 bytes |

FPort 3 is the Bolus configuration **downlink** port and is therefore not decoded as an uplink.

## Telemetry contract

The Telemetry V2 payload contract is frozen for decoder integration and is defined by `App/Services/telemetry_codec.c`.

Byte 31 is the event/reference flag byte, not an application CRC.

Candidate counters and event/reference flags are explicitly emitted as staging/research fields and must not be interpreted as validated physiological diagnoses.

## Validation boundary

The two JavaScript files have been syntax-checked and exercised locally with fixed Telemetry V2 and ACK/NACK vectors.

This is an **offline codec test**, not a LoRaWAN gateway/hardware PASS. OTAA, over-the-air uplink delivery, network-server execution, RX1/RX2 downlink delivery, and FPort-4 return traffic remain to be validated on the real system.
