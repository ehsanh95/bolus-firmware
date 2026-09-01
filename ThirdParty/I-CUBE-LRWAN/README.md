# I-CUBE-LRWAN vendor source

The STM32 I-CUBE-LRWAN source package is vendored here for the Bolus LoRaWAN integration.

Current imported source set:

- `Middlewares/Third_Party/LoRaWAN/` — LoRaMac 4.4.7 / LoRaWAN 1.0.3 middleware, crypto, secure-element software backend, Region implementations and LmHandler.
- `Utilities/` — ST utility sources retained as vendor reference. They are **not** all intended to be linked into the Bolus firmware.
- `Reference/NUCLEO-L476RG/LoRaWAN/LoRaWAN_End_Node/` — STM32L476RG End_Node reference project used only to verify configuration and integration patterns.

The unrelated `LoRaWAN_FUOTA` reference tree is intentionally not kept in this repository because FUOTA/SBSFU is outside the current Phase 5 communication milestone.

## Bolus integration boundary

The existing Bolus RFM95W/SX1276 implementation under `App/Drivers/RF/RFM95W/` remains the authoritative hardware/radio port. Do not replace its pin mapping, SPI implementation or board callbacks with NUCLEO BSP files.

The LoRaWAN integration target is:

`Bolus telemetry -> LoRaWAN service -> LoRaMac -> RegionEU868 -> existing radio.h/SX1276/RFM95W port`

Initial scope:

- EU868 only
- Class A only
- OTAA first
- unconfirmed uplink for the 32-byte telemetry payload
- RX1/RX2 downlink handling
- ADR can be enabled after the first join/uplink/downlink hardware validation

## Build status

The vendor source is present, but the LoRaWAN middleware is not yet declared PASS in the main CubeIDE build. Integration will be enabled incrementally so the Phase 4 radio regression path remains recoverable.

Do not claim LoRaWAN join, gateway acceptance, downlink reception, STOP2 compatibility, or measured power until each item has hardware evidence.
