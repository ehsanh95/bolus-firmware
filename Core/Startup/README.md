# Core/Startup

Startup assembly for the STM32L476RGT6.

| File | Purpose |
|---|---|
| `startup_stm32l476rgtx.s` | Vector table, Reset_Handler, stack initialization, and weak IRQ handlers |

This code runs before `main()`. Strong C definitions can override weak handlers provided by the startup file, for example `RTC_WKUP_IRQHandler`.

Application logic should normally not be added here.
