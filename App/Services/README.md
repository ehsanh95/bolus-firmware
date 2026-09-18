# Services

This directory contains the core application logic. Services turn low-level drivers into product-level workflows.

| File | Purpose |
|---|---|
| `sensor_service.c/.h` | Sensor initialization/readout, power gating, and MPU burst feature extraction |
| `bma_event_service.c/.h` | Any-Motion policy, cooldown handling, and status reads |
| `event_episode_service.c/.h` | Groups motion pulses into Event Episodes and schedules follow-up actions |
| `event_feature_extractor.c/.h` | Online event feature extraction without retaining raw waveforms |
| `event_processor.c/.h` | Rule-based processing/classifier staging |
| `event_aggregator.c/.h` | Event aggregation and temperature history |
| `telemetry_window_service.c/.h` | Window accumulation and frozen telemetry snapshots |
| `telemetry_codec.c/.h` | Versioned telemetry serialization |
| `radio_tx_service.c/.h` | TX queue facade and radio state exposed to main |
| `lorawan_uplink_service.c/.h` | ABP, TX/RX1/RX2, queueing, retry, and downlink routing |
| `downlink_management_service.c/.h` | FPort 3 command decoding and ACK/NACK generation |
| `fault_manager.c/.h` | Active faults, history, and health state |

## Active Data Path

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

Telemetry sizes:
- V2: 32 bytes
- V2.1: 42 bytes
- V2.2: 38 bytes

`lorawan_uplink_service` owns ABP session setup, bounded retry handling, the TX watchdog, RX1/RX2 processing, and application downlink routing.

`RadioTxService_IsRadioCritical()` is used by the scheduler to prevent blocking sensor work from starting during timing-sensitive radio activity.

Some event processor/aggregator/extractor code remains as staging or architectural support and is not necessarily part of the currently active `main.c` path.
