# BMA456

The BMA456 is the primary motion sensor and the **always-on sentinel** in the Bolus low-power architecture.

| File | Type | Purpose |
|---|---|---|
| `bma4.c/.h` | Bosch SensorAPI | Base BMA4 API |
| `bma4_defs.h` | Bosch SensorAPI | Registers, enums, and definitions |
| `bma456h.c/.h` | Bosch SensorAPI | BMA456H feature-engine support |
| `bma456_motion.c/.h` | Bolus wrapper | Low-power initialization, XYZ, and Step Counter |
| `bma456_event.c/.h` | Bolus wrapper | Any-Motion configuration, INT1 mapping, and interrupt status |

`bma456_motion` and `bma456_event` are intentionally separate paths. The motion wrapper handles acceleration and step data, while the event wrapper handles wake/event detection.

The BMA456 remains powered so the Step Counter continues running and Any-Motion can wake the MCU from STOP2.

In the active telemetry path, Step Counter and XYZ are read only when the telemetry snapshot is prepared. There is no 500 ms BMA polling loop.
