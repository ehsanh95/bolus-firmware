# Config

Compile-time hardware settings and runtime application configuration are stored here.

| File | Purpose |
|---|---|
| `bolus_config.h` | RF defaults, power polarity, ADC, TMP117, and fixed timing values |
| `bolus_runtime_config.c/.h` | Runtime configuration, defaults, validation, and event profiles |
| `bolus_lorawan_credentials.h` | LoRaWAN provisioning and credential policy |
| `lorawan_conf.h` | LoRaWAN middleware configuration |
| `mw_log_conf.h` | Middleware logging configuration |
| `utilities_conf.h` | ST utility configuration |

Use `bolus_config.h` for physical hardware and bring-up constants. Use `bolus_runtime_config` for policy that may change at runtime or through downlink commands.

On the low-power branch, the default uplink period is **900 seconds / 15 minutes**.
