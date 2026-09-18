# BSP — Board Support Package

کدهای وابسته به برد Bolus و GPIOهای خاص سخت‌افزار.

| فایل | وظیفه |
|---|---|
| `bolus_power.c/.h` | کنترل railهای TMP117، MPU6050، BMA456، RFM95W و SOC divider |
| `bolus_led.c/.h` | abstraction LEDهای سنسور، MCU و RF |
| `bma_irq_diag.c/.h` | مدیریت EXTI مربوط به BMA456 INT1 و diagnostics |

BMA456 در معماری Low Power معمولاً روشن می‌ماند. MPU6050 در حالت عادی خاموش است و فقط برای burst event روشن می‌شود.

ISR مربوط به BMA456 فقط کار سبک انجام می‌دهد؛ SPI و event processing در main context انجام می‌شوند.
