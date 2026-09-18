# ThirdParty

External middleware dependencies other than the STM32 HAL.

| Directory | Purpose |
|---|---|
| `I-CUBE-LRWAN` | LoRaWAN MAC, Region, Crypto, utilities, and reference code from ST/Semtech |

The package's original README is preserved at `I-CUBE-LRWAN/README.md`.

Vendor middleware should remain unchanged whenever practical. Bolus-specific adaptation should preferably live in:

- `App/Drivers/RF/RFM95W`
- `App/Services/lorawan_uplink_service.*`
- `App/Config/*`

Keeping this separation makes future I-CUBE-LRWAN upgrades easier.
