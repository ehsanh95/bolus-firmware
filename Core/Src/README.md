# Core/Src

کد اجرایی سطح MCU.

| فایل | وظیفه |
|---|---|
| `main.c` | init سخت‌افزار، service orchestration، telemetry schedule و STOP2 |
| `stm32l4xx_hal_msp.c` | setup سطح MSP برای peripheralها |
| `stm32l4xx_it.c` | exception و interrupt handlerهای اصلی |
| `system_stm32l4xx.c` | system clock/Core startup support از ST |
| `syscalls.c` | syscall stubs برای newlib |
| `sysmem.c` | heap و `_sbrk` support |

در شاخه Low Power، `main.c` سنسورها، Event Episode، telemetry 15 دقیقه‌ای، LoRaWAN، IWDG، RTC wake و STOP2 را هماهنگ می‌کند.

قبل از sensor work blocking وضعیت radio critical بررسی می‌شود تا `TxDone` و RX1/RX2 به تأخیر نیفتند.
