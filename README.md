# Bolus Firmware

Firmware پروژه‌ی **Bolus** برای STM32L476RGT6، سنسورها و ارتباط LoRaWAN.

## معماری کلی

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

## سخت‌افزار اصلی

| بخش | قطعه | نقش |
|---|---|---|
| MCU | STM32L476RGT6 | اجرای firmware و Low Power |
| Motion | BMA456 | Any-Motion، Step Counter و XYZ |
| Temperature | TMP117 | اندازه‌گیری دما |
| Burst motion | MPU6050 | burst کوتاه هنگام event |
| Radio | RFM95W / SX1276 | LoRaWAN EU868 Class A |

## ساختار Repository

| مسیر | وظیفه |
|---|---|
| `App/Application` | مدل‌های داده و typeهای سطح application |
| `App/BSP` | کنترل power، LED و IRQ |
| `App/Config` | تنظیمات compile-time و runtime |
| `App/Drivers` | درایورهای باتری، سنسورها و رادیو |
| `App/Services` | منطق سنسور، event، telemetry و LoRaWAN |
| `Core` | startup، main و interruptها |
| `Drivers` | STM32 HAL و CMSIS |
| `ThirdParty` | I-CUBE-LRWAN و middleware خارجی |
| `docs` | اسناد معماری و validation |
| `tests` | تست codec و decoder |
| `tools` | ابزارهای کمکی توسعه |

## Low Power

در شاخه‌ی `phase6/low-power-stop2` MCU بین deadlineها وارد STOP2 می‌شود. BMA456 روشن می‌ماند و با Any-Motion MCU را بیدار می‌کند. MPU6050 فقط هنگام event روشن می‌شود.

## Telemetry

- FPort 2: telemetry
- FPort 3: application downlink
- FPort 4: ACK/NACK
- V2: 32 bytes
- V2.1: 42 bytes
- V2.2: 38 bytes

Step و XYZ مسیر فعال telemetry از **BMA456** می‌آیند، نه MPU6050.
