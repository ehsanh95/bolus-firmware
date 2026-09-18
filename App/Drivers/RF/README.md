# RF Drivers

Radio driver layer for the Bolus firmware.

| Directory | Purpose |
|---|---|
| `RFM95W` | SX1276 driver, board adapter, timer, and system-time support required by LoRaMAC |

LoRaWAN policy is implemented in `App/Services/lorawan_uplink_service.*`. This directory primarily owns direct transceiver access.
