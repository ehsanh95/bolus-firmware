# Bolus Firmware

Firmware for the **Bolus** project, built around the STM32L476RGT6, onboard sensors, and LoRaWAN connectivity.

## High-Level Architecture

```text
BMA456 Any-Motion ─┐
                    ├─→ Core/Src/main.c / Event Manager
TMP117 HIGH/LOW ────┘
        ↓
SensorService
 ├─ BMA456: always-on Step + Any-Motion sentinel
 ├─ TMP117: low-rate continuous HIGH/LOW thermal sentinel
 └─ MPU6050: adaptive, power-gated event burst only
        ↓
EventEpisodeService
 ├─ Motion / Thermal / Combined Episodes
 └─ EventDigestService (RAM)
        ↓
TelemetryWindowService
        ↓
TelemetryCodec V3
        ↓
RadioTxService
        ↓
LoRaWanUplinkService
        ↓
SX1276 / RFM95W
```

## Main Hardware

| Section | Device | Role |
|---|---|---|
| MCU | STM32L476RGT6 | Firmware execution and low-power control |
| Motion | BMA456 | Any-Motion, Step Counter, and XYZ acceleration |
| Temperature | TMP117 | Precision temperature measurement |
| Burst motion | MPU6050 | Short event-driven motion burst capture |
| Radio | RFM95W / SX1276 | LoRaWAN EU868 Class A communication |

## Repository Structure

| Path | Purpose |
|---|---|
| `App/Application` | Application-level data models and shared types |
| `App/BSP` | Power, LED, and board IRQ support |
| `App/Config` | Compile-time and runtime configuration |
| `App/Drivers` | Battery, sensor, and radio drivers |
| `App/Services` | Sensor, event, telemetry, and LoRaWAN logic |
| `Core` | Startup, main loop, and interrupt integration |
| `Drivers` | STM32 HAL and CMSIS dependencies |
| `ThirdParty` | I-CUBE-LRWAN and other external middleware |
| `docs` | Architecture and validation notes |
| `tests` | Codec and decoder tests |
| `tools` | Development helper scripts |

## Low Power

On the `phase6/low-power-stop2` branch, the MCU enters STOP2 between application deadlines. Acquisition Level 0 disables background event wakeups and takes fresh sensor data only at the telemetry boundary. Levels 1..5 use BMA456 Any-Motion plus TMP117 independent HIGH/LOW Alert Mode as dual low-power sentinels. The MPU6050 remains power-gated and is enabled only for selected informative motion bursts.

## Telemetry

- FPort 2: telemetry
- FPort 3: application downlink
- FPort 4: ACK/NACK response
- V2: 32 bytes — legacy/frozen
- V2.1: 42 bytes — legacy/frozen
- V2.2: 38 bytes — legacy/frozen
- V3: active variable-length summary + per-Episode digests, max 51 bytes per packet

V3 sends a 15-minute summary plus as many closed Episode digests as fit in the packet; additional digests use continuation packets with the same window sequence. Step activity is transmitted as a per-window delta from the **BMA456**.
