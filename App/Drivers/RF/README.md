# RF Drivers

لایه‌ی درایور radio پروژه.

| پوشه | وظیفه |
|---|---|
| `RFM95W` | SX1276 driver، board adapter، timer و system-time مورد نیاز LoRaMAC |

LoRaWAN policy در `App/Services/lorawan_uplink_service.*` قرار دارد؛ این پوشه مسئول access مستقیم به transceiver است.
