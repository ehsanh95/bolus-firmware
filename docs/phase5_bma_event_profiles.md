# Phase 5 — BMA456 Event Sensitivity Profiles

This note records the current engineering/field-calibration sweep profiles for the BMA456 Any-Motion event path.

These values are not validated cattle thresholds. They exist only to make animal/bench sweeps repeatable and downlink-ready. Step Counter sensitivity remains a separate configuration path.

| Profile | Any-Motion threshold | Duration | Bundled pre-episode cooldown | Event IRQ |
|---|---:|---:|---:|---|
| VERY_LOW | 900 mg | 800 ms | 0 s | on |
| LOW | 750 mg | 600 ms | 0 s | on |
| LEVEL_1 | 600 mg | 500 ms | 0 s | on |
| LEVEL_2 | 500 mg | 400 ms | 0 s | on — current first in-animal development default |
| LEVEL_3 | 400 mg | 300 ms | 0 s | on |
| LEVEL_4 | 300 mg | 200 ms | 0 s | on |
| RAW | RuntimeConfig | RuntimeConfig | RuntimeConfig | on |
| OFF | n/a | n/a | n/a | off |

`Duration` is the BMA Any-Motion persistence requirement before the event IRQ is produced. It is not the physiological contraction duration. The longer approximately 8–10 s contraction morphology belongs to the later observation/feature layer.

From RuntimeConfig Version 9 onward, bundled sensitivity profiles do not use a long BmaEventService cooldown. The higher-level `EventEpisodeService` must see the timing of motion pulses so it can preserve inter-pulse structure and group them into one Event Episode. RAW mode retains direct cooldown control for bench experiments, but the normal episode path should leave it at zero.

The episode layer applies a separate short retrigger guard (default 2 s) to reject overlapping/chatter pulses. This guard is not the Event Episode boundary. An episode closes only after the configured quiet timeout, default 120 s without an accepted motion pulse.

`OFF` disables the BMA Any-Motion feature mapping and MCU BMA-INT1 harness. It does not disable the BMA sensor itself or the normal scheduled acquisition path.

The 300–900 mg mapping was selected as the first in-animal calibration sweep after review of intrareticular literature and the expected freely rotating/moving bolus environment. Published amplitudes are scale references only and must not be treated as portable thresholds.

All profile mappings, the 2 s retrigger guard, and the 120 s episode quiet timeout remain engineering calibration parameters until synchronized animal data supports final tuning.
