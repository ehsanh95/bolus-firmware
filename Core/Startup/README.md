# Core/Startup

Startup assembly مخصوص STM32L476RGT6.

| فایل | وظیفه |
|---|---|
| `startup_stm32l476rgtx.s` | vector table، Reset_Handler، stack initialization و weak IRQ handlers |

این فایل قبل از `main()` اجرا می‌شود. handlerهای C با strong definition می‌توانند weak handlerهای startup را override کنند؛ برای نمونه `RTC_WKUP_IRQHandler`.

معمولاً logic application نباید در این فایل قرار بگیرد.
