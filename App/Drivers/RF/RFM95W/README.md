# RFM95W / SX1276

Radio driver and Bolus board adapter for the RFM95W module based on the SX1276 transceiver.

| File | Purpose |
|---|---|
| `radio.h` | Generic Radio interface expected by LoRaMAC |
| `sx1276.c/.h` | Main SX1276 driver |
| `sx1276Regs-LoRa.h` | LoRa register and bit definitions |
| `sx1276Regs-Fsk.h` | FSK register and bit definitions |
| `rfm95w_board.c/.h` | SPI/GPIO/EXTI adaptation for the Bolus board |
| `timer.c/.h` | Cooperative timer implementation compatible with LoRaMAC |
| `systime.c/.h` | System-time API required by LoRaMAC |

`rfm95w_board.c` owns SPI transfers, NSS, reset control, DIO interrupt flags, and the HAL bridge.

SX1276 DIO processing is deferred: the EXTI ISR only records a pending interrupt, and the actual radio processing runs in main context. This matters for LoRaWAN Class A because `TxDone` must be processed promptly so RX1 and RX2 are scheduled correctly.

Putting the SX1276 into radio sleep is not the same as physically removing power from the RFM95W rail.
