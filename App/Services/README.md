# Services

هسته‌ی منطق application. Serviceها driverها را به workflow سطح محصول تبدیل می‌کنند.

| فایل | وظیفه |
|---|---|
| `sensor_service.c/.h` | init/read سنسورها، power gating و MPU burst features |
| `bma_event_service.c/.h` | Any-Motion policy، cooldown و status خوانی |
| `event_episode_service.c/.h` | grouping pulseها در Event Episode و follow-upها |
| `event_feature_extractor.c/.h` | استخراج online feature بدون نگهداری waveform خام |
| `event_processor.c/.h` | rule-based processing/classifier staging |
| `event_aggregator.c/.h` | aggregation رخداد و temperature history |
| `telemetry_window_service.c/.h` | جمع‌آوری window و freeze snapshot |
| `telemetry_codec.c/.h` | serialization نسخه‌بندی‌شده |
| `radio_tx_service.c/.h` | facade صف TX و state برای main |
| `lorawan_uplink_service.c/.h` | ABP، TX/RX1/RX2، queue، retry و downlink |
| `downlink_management_service.c/.h` | فرمان‌های FPort 3 و ACK/NACK |
| `fault_manager.c/.h` | fault، history و health state |

## مسیر فعال

```text
BMA456 IRQ
   ↓
BmaEventService
   ↓
EventEpisodeService
   ├─ TMP117 one-shot
   └─ MPU6050 burst
   ↓
TelemetryWindowService
   ↓
TelemetryCodec V2.2
   ↓
RadioTxService
   ↓
LoRaWanUplinkService
```

Telemetry V2 = 32B، V2.1 = 42B و V2.2 = 38B.

`lorawan_uplink_service` مسئول ABP session، bounded retry، TX watchdog، RX1/RX2 و routing downlink است. `RadioTxService_IsRadioCritical()` مانع شروع sensor work blocking در window حساس radio می‌شود.

برخی فایل‌های event processor/aggregator/extractor برای staging و توسعه نگهداری شده‌اند و الزاماً در مسیر فعال `main.c` نیستند.
