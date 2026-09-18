# Application

Application-level data contracts live here. These structures are **not direct LoRaWAN wire formats**.

| File | Purpose |
|---|---|
| `bolus_types.h` | State, operating mode, health, validity flags, and measurement models |
| `event_data.h` | Event feature records and temperature trajectory data |
| `telemetry_data.h` | Internal Telemetry V1/V2/V2.1/V2.2 models |

`telemetry_data.h` defines the source model consumed by the encoder. The actual byte layout is defined in `App/Services/telemetry_codec.*`.

The BMA456 snapshot used by V2.1/V2.2 contains the native Step Counter and XYZ acceleration values.
