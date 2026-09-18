# TMP117

TMP117 driver for precision temperature measurement over I2C.

| File | Purpose |
|---|---|
| `tmp117.c` | Register access and temperature conversion implementation |
| `tmp117.h` | Register definitions, operating modes, and public API |

In the Bolus architecture, the sensor normally remains in Shutdown Mode and is read using one-shot conversions:

```text
Shutdown → One-Shot → wait DRDY → read → Shutdown
```

The base driver is generic. Timeout handling, validation, fault reporting, and scheduling are implemented by `SensorService`.
