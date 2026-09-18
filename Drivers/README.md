# STM32 Drivers

Official ST dependencies for the MCU. These directories are generally not application-owned.

| Directory | Purpose |
|---|---|
| `CMSIS` | Cortex/STM32 device definitions and core APIs |
| `STM32L4xx_HAL_Driver` | STM32 HAL headers and implementations |

Avoid direct vendor-code modifications whenever possible. Product changes should normally be implemented in `Core`, `App/BSP`, or `App/Drivers` so future CubeMX regeneration and vendor upgrades remain manageable.
