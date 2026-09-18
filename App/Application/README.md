# Application

قراردادهای داده‌ی سطح application. این structها **wire format مستقیم LoRaWAN نیستند**.

| فایل | وظیفه |
|---|---|
| `bolus_types.h` | state، operating mode، health، validity و measurement model |
| `event_data.h` | feature recordهای event و trajectory دما |
| `telemetry_data.h` | مدل‌های داخلی Telemetry V1/V2/V2.1/V2.2 |

`telemetry_data.h` مدل منبع encoder است؛ byte layout واقعی در `App/Services/telemetry_codec.*` تعریف می‌شود. BMA456 snapshot در V2.1/V2.2 شامل Step Counter و XYZ است.
