# STM32L4 HAL Driver

HAL رسمی ST برای خانواده STM32L4.

| مسیر/فایل | وظیفه |
|---|---|
| `Inc` | header و APIهای HAL |
| `Src` | implementation GPIO، SPI، I2C، ADC، PWR، RCC و سایر peripheralها |
| `LICENSE.txt` | license |

توابعی مانند `HAL_PWREx_EnterSTOP2Mode()` از این dependency می‌آیند، اما policy ورود به STOP2 در `Core/Src/main.c` قرار دارد.
