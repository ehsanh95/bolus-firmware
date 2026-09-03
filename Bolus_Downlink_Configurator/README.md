# Bolus Payload Toolkit

Status: **IMPLEMENTED / OFFLINE TOOLING TESTED / LORAWAN END-TO-END UNTESTED**

`Bolus_Downlink_Configurator/index.html` is a dependency-free browser tool for the current Bolus application payload contracts.

It now provides two functions in one fully English offline UI:

1. **Downlink Builder** — creates configuration commands for LoRaWAN **FPort 3**.
2. **Uplink Decoder** — decodes Telemetry V2 on **FPort 2** and downlink ACK/NACK responses on **FPort 4**.

No installation, network connection, or device credentials are required.

## Run

1. Pull the branch.
2. Open `Bolus_Downlink_Configurator/index.html` in Chrome, Edge, Firefox, or another modern browser.
3. Use **Downlink Builder** to create configuration payloads.
4. Use **Uplink Decoder** to inspect raw HEX or Base64 application uplinks.

## Application ports

| FPort | Direction | Purpose |
|---:|---|---|
| 2 | Bolus -> server | Telemetry V2 summary |
| 3 | Server -> Bolus | Configuration downlink |
| 4 | Bolus -> server | Configuration ACK/NACK |

## Telemetry V2 decoder

The decoder follows the exact firmware encoder in `App/Services/telemetry_codec.c`.

Current Telemetry V2 contract:

- fixed size: **32 bytes**;
- byte 0: version/message header, current summary = `0x21`;
- bytes 1-2: sequence, little-endian;
- byte 3: RuntimeConfig version;
- byte 4: status/validity flags;
- bytes 5-7: battery percentage and mV;
- bytes 8-15: current/min/max/negative-excursion temperature fields;
- bytes 16-22: episode, pulse, interval, and MPU burst counters;
- bytes 23-28: quantized MPU features;
- bytes 29-30: reserved classifier candidate counters;
- byte 31: low 8 bits of event/reference flags (**not CRC**).

The UI expands quantized MPU fields into engineering units (mg, dps, degrees) and signed temperature values into degrees Celsius.

Candidate counters and event/reference flags are research/staging fields. They must not be interpreted as validated physiological diagnoses.

## Downlink Builder

Tick only the settings that must change, select a transaction ID, and press **Generate Payload**.

Outputs:

- space-separated HEX;
- compact HEX;
- Base64;
- required application port: **FPort 3**.

The request protocol is versioned TLV:

```text
byte 0    0xD1 request magic
byte 1    protocol version = 1
byte 2    transaction id
byte 3    TLV command count
byte 4+   [command id][length][value...] repeated
```

Current groups:

- Motion / Event
- Temperature / MPU
- Telemetry period
- RF / radio policy

BMA Event sensitivity and BMA Step sensitivity are intentionally independent settings.

## ACK/NACK decoder

FPort 4 control responses are fixed at 8 bytes:

```text
byte 0    0xD2 response magic
byte 1    protocol version = 1
byte 2    transaction id
byte 3    result code
byte 4-5  apply mask, little-endian
byte 6-7  RuntimeConfig version, little-endian
```

The UI expands the apply mask into the current pending subsystems:

- `BMA_EVENT`
- `BMA_SENSOR`
- `EVENT_EPISODE`
- `MPU_SENSOR`
- `TELEMETRY_WINDOW`
- `RADIO_POLICY`

## Network-server decoders

Ready-to-paste, commented JavaScript decoders are stored in:

`Network_Decoders/`

- `the_things_stack_uplink_decoder.js`
- `chirpstack_uplink_decoder.js`

Both decode FPort 2 Telemetry V2 and FPort 4 ACK/NACK using the same firmware wire contract.

## Validation boundary

The decoder JavaScript has been syntax-checked and exercised against fixed local vectors. This validates the **offline codec logic only**.

It does **not** prove:

- a clean CubeIDE build of the current LoRaWAN integration;
- OTAA join with real credentials;
- gateway/network-server delivery;
- RX1/RX2 downlink behavior;
- FPort 4 ACK/NACK transmission over the air;
- live application of cached RF/service settings;
- persistence across reset/power loss;
- STOP2 / RTC behavior or current consumption.

The LoRaWAN communication path therefore remains **IMPLEMENTED / UNTESTED / NOT HARDWARE VALIDATED** until the corresponding tests are recorded.

## Sources of truth

Firmware protocol definitions remain authoritative:

- `App/Services/telemetry_codec.c`
- `App/Services/telemetry_codec.h`
- `App/Services/downlink_management_service.h`
- `App/Config/bolus_lorawan_credentials.h`

Keep the browser toolkit and network-server decoder files synchronized whenever these contracts change.
