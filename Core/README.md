# Core

MCU-level project code, largely generated or structured around STM32CubeMX/CubeIDE, plus the application integration points.

| Directory | Purpose |
|---|---|
| `Inc` | Main MCU/HAL headers |
| `Src` | Main loop, interrupts, MSP, and system support |
| `Startup` | Vector table and reset startup assembly |

Product logic should stay in `App` whenever possible. `Core/Src/main.c` coordinates the top-level application flow and the low-power scheduler.
