# Bolus Payload Toolkit

Status: **IMPLEMENTED / OFFLINE TOOLING TESTED / LORAWAN END-TO-END UNTESTED**

`Bolus_Downlink_Configurator/index.html` is a dependency-free browser tool for the current Bolus application payload contracts.

It now provides two functions in one fully English offline UI:

1. **Downlink Builder** — creates configuration commands for LoRaWAN **FPort 3**.
2. **Uplink Decoder** — decodes Telemetry V2/V2.1/V2.2 on **FPort 2** and downlink ACK/NACK responses on **FPort 4**.

No installation, network connection, or device credentials are required.

## Run

1. Pull the branch.
2. Open `Bolus_Downlink_Configurator/index.html` in Chrome, Edge, Firefox, or another modern browser.
3. Use **Downlink Builder** to create configuration payloads.
4. Use **Uplink Decoder** to inspect raw HEX or Base64 application uplinks.

## Application ports

| FPort | Direction | Purpose |
|---:|---|---|
| 2 | Bolus -> server | Telemetry V2/V2.1/V2.2 summary |
| 3 | Server -> Bolus | Configuration downlink |
| 4 | Bolus -> server | Configuration ACK/NACK |

## Telemetry decoder

The decoder follows the exact firmware encoder in `App/Services/telemetry_codec.c`.

Current active Telemetry V2.2 contract:

- fixed size: **38 bytes**;
- byte 0: version/message header, current summary = `0x41`;
- bytes 1-2: sequence, little-endian;
- byte 3: RuntimeConfig version;
- byte 4: status/validity flags;
- bytes 5-6: battery voltage in mV;
- bytes 7-14: current/min/max/negative-excursion temperature fields;
- bytes 15-21: episode, pulse, interval, and MPU burst counters;
- bytes 22-27: quantized MPU features;
- bytes 28-31: native BMA456 Step Counter;
- bytes 32-37: native BMA456 XYZ acceleration in mg.

The UI expands quantized MPU fields into engineering units (mg, dps, degrees) and signed temperature values into degrees Celsius. V2.2 reports one representation per measurement: battery voltage only in millivolts and temperature only in degrees Celsius.

Frozen V2 (32-byte) and V2.1 (42-byte) payloads remain supported for backward compatibility. Their legacy decoded shape is preserved.

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

Both decode FPort 2 Telemetry V2/V2.1/V2.2 and FPort 4 ACK/NACK using the same firmware wire contract.

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
