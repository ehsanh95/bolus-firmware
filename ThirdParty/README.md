# ThirdParty

Dependencyهای خارجی غیر از STM32 HAL.

| پوشه | وظیفه |
|---|---|
| `I-CUBE-LRWAN` | LoRaWAN MAC، Region، Crypto، utilityها و reference code از ST/Semtech |

README اصلی package داخل `I-CUBE-LRWAN/README.md` نیز نگهداری شده است.

تا جای ممکن middleware vendor دست‌نخورده باقی بماند. adaptation پروژه بهتر است در این مسیرها انجام شود:

- `App/Drivers/RF/RFM95W`
- `App/Services/lorawan_uplink_service.*`
- `App/Config/*`

این جداسازی upgrade نسخه‌های بعدی I-CUBE-LRWAN را ساده‌تر می‌کند.
