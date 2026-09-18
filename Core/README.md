# Core

بخش MCU-level پروژه که عمدتاً توسط STM32CubeMX/CubeIDE ساخته شده و نقاط اتصال application را فراهم می‌کند.

| پوشه | وظیفه |
|---|---|
| `Inc` | headerهای اصلی MCU/HAL |
| `Src` | main، interruptها، MSP و system support |
| `Startup` | vector table و reset startup assembly |

منطق محصول تا جای ممکن باید در `App` بماند. `Core/Src/main.c` orchestration اصلی و Low Power scheduler را اجرا می‌کند.
