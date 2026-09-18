# MPU6050

MPU6050 در Bolus یک سنسور **burst-only** است، نه motion sensor دائمی.

| فایل | وظیفه |
|---|---|
| `MPU6050.c/.h` | driver پایه I2C و accel/gyro/temp/sleep/wake |
| `mpu6050_motion.c/.h` | wrapper پروژه برای scaling، config و burst sampling |

```text
BMA event accepted
      ↓
MPU rail ON
      ↓
initialize / wake
      ↓
bounded burst
      ↓
feature extraction
      ↓
software sleep
      ↓
rail OFF
```

featureهای burst شامل peak/RMS acceleration، angular velocity، total angular motion و تغییر orientation هستند.

MPU6050 نباید منبع Step Counter یا XYZ telemetry باشد؛ این داده‌ها متعلق به BMA456 هستند.
