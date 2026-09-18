# Sensor Drivers

درایورهای سنسورهای Bolus.

| پوشه | سنسور | نقش اصلی |
|---|---|---|
| `BMA456` | Bosch BMA456 | motion اصلی، Step Counter و Any-Motion |
| `TMP117` | TI TMP117 | دمای دقیق |
| `MPU6050` | MPU6050 | burst کوتاه برای featureهای حرکتی/چرخشی |

```text
BMA456: ON / sentinel
TMP117: shutdown + one-shot
MPU6050: rail OFF → event → burst → OFF
```

Step و XYZ telemetry از BMA456 گرفته می‌شوند. MPU6050 منبع اصلی step/accel telemetry نیست.
