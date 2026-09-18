# Tests

تست‌های مستقل از firmware target.

| فایل | وظیفه |
|---|---|
| `test_telemetry_codec_v2_2.c` | بررسی byte layout و encoding Telemetry V2.2 |
| `test_uplink_decoders.js` | تست decoderهای uplink با vectorهای مشخص |

هدف اصلی جلوگیری از شکستن قرارداد wire بین firmware و ChirpStack decoder است.

تغییر یک field telemetry باید با تست encoder، تست decoder و حفظ compatibility نسخه‌های frozen همراه باشد.
