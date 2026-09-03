# Bolus Downlink Configurator

Status: **STAGING / UNTESTED**

This is a dependency-free offline helper for building the current Bolus LoRaWAN application downlink protocol.

## Run

No installation is required.

1. Pull the branch.
2. Open `Bolus_Downlink_Configurator/index.html` in a browser.
3. Choose a transaction ID.
4. Tick only the settings that must change.
5. Enter/select the new values.
6. Press **ساخت payload**.
7. Send the generated HEX or Base64 as a LoRaWAN application downlink on **FPort 3**.
8. The firmware is designed to return its 8-byte ACK/NACK on **FPort 4**. The page also contains an ACK/NACK decoder.

The page works locally/offline and does not contain network credentials.

## Important validation boundary

Generating a payload does **not** prove that the downlink path works end-to-end.

As of this checkpoint, the following remain unvalidated:

- clean CubeIDE build after the LoRaWAN/downlink integration;
- OTAA with real credentials;
- gateway/network-server RX1/RX2 delivery;
- FPort-3 command reception on hardware;
- FPort-4 ACK/NACK transmission;
- cached-service live reconfiguration;
- RF/LoRaWAN MAC live application of the new radio settings;
- persistence across reset/power loss.

The firmware source of truth is:

`App/Services/downlink_management_service.h`

and the final candidate configuration is validated by:

`BolusRuntimeConfig_Validate()`.

## Current groups

The UI currently covers the downlink-addressable fields in these groups:

- Motion / Event
- Temperature
- MPU
- Telemetry period
- RF / radio policy

BMA Event sensitivity and BMA Step sensitivity are intentionally separate settings.

## RF note

The RF controls modify the corresponding `RuntimeConfig.radio` fields and set the `DOWNLINK_APPLY_RADIO_POLICY` pending bit.

They are therefore **command-decodable but not yet claimed to be live LoRaWAN PHY settings**. In particular, the imported EU868 LoRaWAN region derives modulation parameters from LoRaWAN data rate; live mapping/application still needs a separate implementation and hardware/network validation milestone.

## Example

Transaction `0x2A`, with:

- BMA Event sensitivity = LEVEL_3 (`5`)
- Uplink period = 900 s
- TX power = 10 dBm
- SF = 7
- Retry delay = 2000 ms
- Max attempts = 3

produces:

```text
D1 01 2A 06 01 01 05 07 04 84 03 00 00 0A 01 0A 0B 01 07 0F 02 D0 07 10 01 03
```

Compact HEX:

```text
D1012A060101050704840300000A010A0B01070F02D007100103
```

This vector is a protocol construction example only; it is **not** an end-to-end hardware PASS result.
