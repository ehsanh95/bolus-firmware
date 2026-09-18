# BSP — Board Support Package

Board-specific Bolus code and hardware GPIO ownership live here.

| File | Purpose |
|---|---|
| `bolus_power.c/.h` | Controls the TMP117, MPU6050, BMA456, RFM95W, and SOC-divider power domains |
| `bolus_led.c/.h` | Abstraction for sensor, MCU, and RF LEDs |
| `bma_irq_diag.c/.h` | BMA456 INT1 EXTI handling and IRQ diagnostics |

In the low-power architecture, the BMA456 normally remains powered. The MPU6050 is normally off and is enabled only for event bursts.

The BMA456 ISR performs only minimal work; SPI access and event processing are deferred to main context.
