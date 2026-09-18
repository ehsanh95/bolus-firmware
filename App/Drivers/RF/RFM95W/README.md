# RFM95W / SX1276

درایور radio و adapter پروژه برای ماژول RFM95W مبتنی بر SX1276.

| فایل | وظیفه |
|---|---|
| `radio.h` | interface عمومی Radio مورد انتظار LoRaMAC |
| `sx1276.c/.h` | driver اصلی SX1276 |
| `sx1276Regs-LoRa.h` | register/bit definitions حالت LoRa |
| `sx1276Regs-Fsk.h` | register/bit definitions حالت FSK |
| `rfm95w_board.c/.h` | اتصال driver به SPI/GPIO/EXTI برد Bolus |
| `timer.c/.h` | timer cooperative سازگار با LoRaMAC |
| `systime.c/.h` | system-time API مورد نیاز LoRaMAC |

`rfm95w_board.c` مسئول SPI، NSS، Reset، DIO flags و bridge به HAL است.

DIO handlerهای SX1276 داخل EXTI اجرا نمی‌شوند؛ ISR فقط pending flag می‌گذارد و پردازش واقعی در main context انجام می‌شود. این موضوع برای Class A مهم است چون `TxDone` باید سریع پردازش شود تا RX1/RX2 به‌موقع باز شوند.

خواب داخلی SX1276 با خاموش‌کردن کامل rail RFM95W متفاوت است.
