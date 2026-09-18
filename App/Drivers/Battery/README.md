# Battery Driver

اندازه‌گیری ولتاژ باتری از طریق ADC و divider قابل کنترل.

| فایل | وظیفه |
|---|---|
| `battery.c` | ADC calibration، فعال‌سازی divider، conversion و تخمین درصد |
| `battery.h` | API و status codeها |

```text
SOC divider ON
   ↓
settling delay
   ↓
ADC conversion
   ↓
raw → ADC mV → battery mV
   ↓
SOC divider OFF
```

divider فقط هنگام اندازه‌گیری روشن می‌شود. جدول voltage-to-SOC فعلی bring-up estimate است و باید با discharge واقعی باتری کالیبره شود.
