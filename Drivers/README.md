# STM32 Drivers

Dependencyهای رسمی ST برای MCU؛ عمدتاً application-owned نیستند.

| پوشه | وظیفه |
|---|---|
| `CMSIS` | تعریف Cortex/STM32 device و core API |
| `STM32L4xx_HAL_Driver` | STM32 HAL implementation/headerها |

تا جای ممکن vendor files را مستقیم تغییر ندهید. تغییر application بهتر است در `Core`، `App/BSP` یا `App/Drivers` انجام شود تا upgrade/regeneration ساده بماند.
