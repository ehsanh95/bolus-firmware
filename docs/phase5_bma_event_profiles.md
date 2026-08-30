# Phase 5 — BMA456 Event Sensitivity Profiles

This note records the current engineering/field-calibration sweep profiles for the BMA456 Any-Motion event path.

These values are not validated cattle thresholds. They exist only to make animal/bench sweeps repeatable and downlink-ready. Step Counter sensitivity remains a separate configuration path.

| Profile | Any-Motion threshold | Duration | Cooldown | Event IRQ |
|---|---:|---:|---:|---|
| VERY_LOW | 900 mg | 800 ms | 60 s | on |
| LOW | 750 mg | 600 ms | 45 s | on |
| LEVEL_1 | 600 mg | 500 ms | 30 s | on |
| LEVEL_2 | 500 mg | 400 ms | 15 s | on — current first in-animal development default |
| LEVEL_3 | 400 mg | 300 ms | 5 s | on |
| LEVEL_4 | 300 mg | 200 ms | 0 s | on |
| RAW | RuntimeConfig | RuntimeConfig | RuntimeConfig | on |
| OFF | n/a | n/a | n/a | off |

`Duration` is the BMA Any-Motion persistence requirement before the event IRQ is produced. It is not the physiological contraction duration. The longer approximately 8–10 s contraction morphology belongs to the later observation/feature layer.

`Cooldown` is software suppression after one accepted event. It is separate from the Any-Motion duration and must stay short enough not to hide the approximately 40–60 s recurrence structure used later for motility features.

`OFF` disables the BMA Any-Motion feature mapping and MCU BMA-INT1 harness. It does not disable the BMA sensor itself or the normal scheduled acquisition path. The product scheduler will later use the normal scheduled acquisition at the configured interval (15 min default uplink cadence); the current `main.c` still contains temporary fast bench reads until that scheduler migration is complete.

The 300–900 mg mapping was selected as the first in-animal calibration sweep after review of intrareticular literature and the expected freely rotating/moving bolus environment. Published amplitudes are scale references only and must not be treated as portable thresholds.

All profile mappings must remain field-calibrated before being treated as biological thresholds.
