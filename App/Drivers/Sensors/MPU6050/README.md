# MPU6050

The MPU6050 is used as a **burst-only** sensor in Bolus, not as a continuously active motion source.

| File | Purpose |
|---|---|
| `MPU6050.c/.h` | Base I2C driver and accel/gyro/temp/sleep/wake functions |
| `mpu6050_motion.c/.h` | Bolus wrapper for scaling, configuration, and burst sampling |

Typical event flow:

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

Burst features include peak/RMS acceleration, angular velocity, total angular motion, and orientation change.

The MPU6050 must not be used as the source of Step Counter or primary XYZ telemetry. Those fields belong to the BMA456 path.
