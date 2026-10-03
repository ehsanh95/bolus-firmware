# Bolus Payload Toolkit

Status: **OFFLINE PROTOCOL TOOLING VERIFIED WITH VECTORS / LIVE DOWNLINK AND APPLY UNDER BENCH VALIDATION**

`Bolus_Downlink_Configurator/index.html` is a dependency-free browser tool for the current Bolus application payload contracts.

It now provides two functions in one fully English offline UI:

1. **Downlink Builder** — creates validated configuration commands for LoRaWAN **FPort 3**, with acquisition presets preceding explicit overrides and a conservative 51-byte bench payload ceiling.
2. **Uplink Decoder** — decodes active Telemetry **V3**, legacy V2/V2.1/V2.2 on **FPort 2**, and downlink ACK/NACK responses on **FPort 4**.

No installation, network connection, or device credentials are required.

## Run

1. Pull the branch.
2. Open `Bolus_Downlink_Configurator/index.html` in Chrome, Edge, Firefox, or another modern browser.
3. Use **Downlink Builder** to create configuration payloads.
4. Use **Uplink Decoder** to inspect raw HEX or Base64 application uplinks.

## Application ports

| FPort | Direction | Purpose |
|---:|---|---|
| 2 | Bolus -> server | Active Telemetry V3 plus legacy V2/V2.1/V2.2 |
| 3 | Server -> Bolus | Configuration downlink |
| 4 | Bolus -> server | Configuration ACK/NACK |

## Telemetry decoder

The decoder follows the exact firmware encoder in `App/Services/telemetry_codec.c`.

Legacy Telemetry V2.2 contract (active firmware uses variable-length V3 summaries and digest continuation packets):

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

Tick only the settings that must change, select a fresh transaction ID for each new request, and press **Generate Payload**. The firmware suppresses only the most recently accepted transaction ID, even if a repeated ID carries different values. Reuse an ID only for intentional duplicate testing. Wait for the FPort 4 response before sending another request.

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

BMA Event sensitivity and BMA Step sensitivity are intentionally independent settings. When a packet includes acquisition level and explicit sensor/event overrides, the builder emits the acquisition preset first so the later overrides are not silently reset.

The builder rejects request payloads above **51 bytes** for conservative bench testing; actual delivery limits can be lower depending on regional data rate and MAC options. Split larger edits into separately acknowledged transactions. BW250 requires explicitly selecting SF7 in the same packet. RuntimeConfig performs final whole-configuration validation; the browser cannot know every setting already present on the board.

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

The UI expands the response apply mask into subsystems requested by the command. **This is not a snapshot of which hardware settings have actually been applied.** Confirm the device's `pending_apply_mask`, `failed_apply_mask`, and readback/behavior:

- `BMA_EVENT`
- `BMA_SENSOR`
- `EVENT_EPISODE`
- `MPU_SENSOR`
- `TELEMETRY_WINDOW`
- `RADIO_POLICY`
- `TMP_SENSOR`

## Network-server decoders

Ready-to-paste, commented JavaScript decoders are stored in:

`Network_Decoders/`

- `the_things_stack_uplink_decoder.js`
- `chirpstack_uplink_decoder.js`

Both decode FPort 2 Telemetry V2/V2.1/V2.2 and FPort 4 ACK/NACK using the same firmware wire contract.

## Validation boundary

The decoder JavaScript has been syntax-checked and exercised against fixed local vectors. This validates the **offline codec logic only**.

It does **not** prove:

- a clean CubeIDE build for every subsequent firmware change;
- a fresh LoRaWAN 1.0.3 ABP session after reset (frame counters are not persisted);
- gateway delivery of the particular downlink under test;
- RX1/RX2 downlink behavior;
- FPort 4 ACK/NACK transmission over the air;
- live application of cached RF/service settings;
- persistence across reset/power loss;
- STOP2 / RTC behavior or current consumption.

An FPort 2 uplink has been observed on a bench board; do not infer FPort 3 downlink reception, FPort 4 response transmission, RX slot behavior, or hardware apply from that result.

Run `node tests/test_downlink_configurator.js` for offline builder regression coverage. This test is included in `bash tests/run_host_tests.sh` and does not replace live board validation.

## Sources of truth

Firmware protocol definitions remain authoritative:

- `App/Services/telemetry_codec.c`
- `App/Services/telemetry_codec.h`
- `App/Services/downlink_management_service.h`
- `App/Config/bolus_lorawan_credentials.h`

Keep the browser toolkit and network-server decoder files synchronized whenever these contracts change.
