# App

کدهای اختصاصی محصول Bolus در این پوشه قرار دارند.

| پوشه | وظیفه |
|---|---|
| `Application` | قراردادهای داده و typeهای مشترک |
| `BSP` | abstraction برد، GPIO و power |
| `Config` | تنظیمات سخت‌افزار و runtime |
| `Drivers` | دسترسی سطح پایین به قطعات |
| `Services` | منطق سطح بالای محصول |

اصل معماری:

```text
Application models
       ↑
Services
       ↑
Drivers / BSP
       ↑
STM32 HAL
```

Driverها باید تا حد ممکن policy محصول را ندانند؛ زمان‌بندی، aggregation، retry و fault policy در Serviceها قرار می‌گیرد.
