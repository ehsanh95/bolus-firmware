# Core/Src

MCU-level executable source files.

| File | Purpose |
|---|---|
| `main.c` | Hardware initialization, service orchestration, telemetry scheduling, and STOP2 control |
| `stm32l4xx_hal_msp.c` | MSP-level peripheral setup |
| `stm32l4xx_it.c` | Main exception and interrupt handlers |
| `system_stm32l4xx.c` | ST system clock and Cortex startup support |
| `syscalls.c` | newlib syscall stubs |
| `sysmem.c` | Heap and `_sbrk` support |

On the low-power branch, `main.c` coordinates sensors, Event Episodes, 15-minute telemetry, LoRaWAN processing, IWDG refresh, RTC wake scheduling, and STOP2 entry/exit.

Before starting blocking sensor work, the main loop checks the radio-critical state so `TxDone` processing and RX1/RX2 timing are not delayed.
