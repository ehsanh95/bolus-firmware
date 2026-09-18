# STM32L4 HAL Driver

Official ST HAL implementation for the STM32L4 family.

| Path / File | Purpose |
|---|---|
| `Inc` | HAL headers and public APIs |
| `Src` | GPIO, SPI, I2C, ADC, PWR, RCC, and other peripheral implementations |
| `LICENSE.txt` | License information |

Functions such as `HAL_PWREx_EnterSTOP2Mode()` come from this dependency, while the policy that decides **when** to enter STOP2 lives in `Core/Src/main.c`.
