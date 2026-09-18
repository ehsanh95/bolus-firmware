# Sensor Drivers

Sensor drivers used by the Bolus firmware.

| Directory | Sensor | Primary Role |
|---|---|---|
| `BMA456` | Bosch BMA456 | Main motion source, Step Counter, and Any-Motion |
| `TMP117` | TI TMP117 | Precision temperature sensing |
| `MPU6050` | MPU6050 | Short event-driven motion/orientation bursts |

Power architecture:

```text
BMA456: ON / sentinel
TMP117: shutdown + one-shot
MPU6050: rail OFF → event → burst → OFF
```

Step Counter and XYZ telemetry are sourced from the BMA456. The MPU6050 is not the primary source for step or acceleration telemetry.
