# Config

تنظیمات ثابت سخت‌افزار و configuration قابل تغییر application.

| فایل | وظیفه |
|---|---|
| `bolus_config.h` | RF defaults، polarity تغذیه، ADC، TMP117 و timingهای ثابت |
| `bolus_runtime_config.c/.h` | runtime config، defaults، validation و event profiles |
| `bolus_lorawan_credentials.h` | provisioning و policy LoRaWAN |
| `lorawan_conf.h` | تنظیم middleware LoRaWAN |
| `mw_log_conf.h` | logging middleware |
| `utilities_conf.h` | تنظیم utilityهای ST |

`bolus_config.h` برای خصوصیات فیزیکی/bring-up است و `bolus_runtime_config` برای policyهایی که ممکن است در runtime یا downlink تغییر کنند.

در شاخه Low Power، uplink پیش‌فرض **900 ثانیه / 15 دقیقه** است.
