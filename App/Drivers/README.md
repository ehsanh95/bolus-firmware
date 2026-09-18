# App Drivers

Project-specific hardware drivers and adapters.

| Directory | Device / Area | Purpose |
|---|---|---|
| `Battery` | ADC + divider | Battery voltage measurement |
| `RF/RFM95W` | SX1276 / RFM95W | Radio driver and board adaptation |
| `Sensors/BMA456` | BMA456 | Motion, Step Counter, and Any-Motion |
| `Sensors/MPU6050` | MPU6050 | Burst motion and orientation capture |
| `Sensors/TMP117` | TMP117 | Precision temperature sensing |

Drivers provide device access. Scheduling and product-level policy belong in `App/Services`.
