# Core/Inc

Headerهای اصلی STM32 application.

| فایل | وظیفه |
|---|---|
| `main.h` | pin mapping، peripheral declarations و compatibility definitions |
| `stm32l4xx_hal_conf.h` | فعال/غیرفعال کردن moduleهای HAL |
| `stm32l4xx_it.h` | prototype interrupt handlerها |

این فایل‌ها به CubeMX/CubeIDE نزدیک هستند. پس از regeneration باید تغییرات دستی و compatibility patchها بررسی شوند.
