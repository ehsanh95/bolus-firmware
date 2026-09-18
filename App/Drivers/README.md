# App Drivers

Driverها و adapterهای سخت‌افزاری پروژه.

| پوشه | قطعه/بخش | وظیفه |
|---|---|---|
| `Battery` | ADC + divider | ولتاژ باتری |
| `RF/RFM95W` | SX1276/RFM95W | radio driver و board adaptation |
| `Sensors/BMA456` | BMA456 | motion، step و Any-Motion |
| `Sensors/MPU6050` | MPU6050 | burst motion/orientation |
| `Sensors/TMP117` | TMP117 | دمای دقیق |

Driver دسترسی به قطعه را فراهم می‌کند؛ policy زمان‌بندی و تصمیم‌های محصول در `App/Services` قرار می‌گیرند.
