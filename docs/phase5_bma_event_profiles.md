# Phase 5 — BMA456 Event Sensitivity Profiles

This note records the current engineering/bench sweep profiles for the BMA456 Any-Motion event path.

These values are not validated cattle thresholds. They exist only to make animal/bench sweeps repeatable and downlink-ready. Step Counter sensitivity remains a separate configuration path.

| Profile | Any-Motion threshold | Duration | Cooldown | Event IRQ |
|---|---:|---:|---:|---|
| VERY_LOW | 600 mg | 600 ms | 60 s | on |
| LOW | 400 mg | 400 ms | 45 s | on |
| LEVEL_1 | 300 mg | 300 ms | 30 s | on |
| LEVEL_2 | 200 mg | 200 ms | 15 s | on — current development default |
| LEVEL_3 | 150 mg | 140 ms | 5 s | on |
| LEVEL_4 | 100 mg | 100 ms | 0 s | on — approximately prior bench readback |
| RAW | RuntimeConfig | RuntimeConfig | RuntimeConfig | on |
| OFF | n/a | n/a | n/a | off |

`OFF` disables the BMA Any-Motion feature mapping and MCU BMA-INT1 harness. It does not disable the BMA sensor itself or the normal scheduled acquisition path. The product scheduler will later use the normal scheduled acquisition at the configured interval (15 min default uplink cadence); the current `main.c` still contains temporary fast bench reads until that scheduler migration is complete.

All profile mappings must remain field-calibrated before being treated as biological thresholds.
