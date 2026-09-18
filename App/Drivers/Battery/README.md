# Battery Driver

Battery voltage measurement using the ADC and a controllable resistor divider.

| File | Purpose |
|---|---|
| `battery.c` | ADC calibration, divider control, conversion, and percentage estimation |
| `battery.h` | Driver API and status codes |

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

The divider is enabled only during a measurement to avoid continuous current draw.

The current voltage-to-SOC table is a bring-up estimate and should be calibrated against the actual Bolus battery discharge curve.
