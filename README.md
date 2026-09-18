# Bolus Firmware

Firmware for the **Bolus** project, built around the STM32L476RGT6, onboard sensors, and LoRaWAN connectivity.

## High-Level Architecture

```text
BMA456 Any-Motion / RTC wake
        ↓
Core/Src/main.c
        ↓
SensorService
 ├─ BMA456: Step + XYZ snapshot
 ├─ TMP117: temperature one-shot
 └─ MPU6050: event-driven burst only
        ↓
EventEpisodeService
        ↓
TelemetryWindowService
        ↓
TelemetryCodec
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

On the `phase6/low-power-stop2` branch, the MCU enters STOP2 between application deadlines. The BMA456 remains powered and can wake the MCU through Any-Motion. The MPU6050 is powered only for event bursts.

## Telemetry

- FPort 2: telemetry
- FPort 3: application downlink
- FPort 4: ACK/NACK response
- V2: 32 bytes
- V2.1: 42 bytes
- V2.2: 38 bytes

Step Counter and XYZ values in the active telemetry path come from the **BMA456**, not the MPU6050.
