#ifndef BMA_IRQ_DIAG_H
#define BMA_IRQ_DIAG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * STEP-2 bring-up harness.
 *
 * This intentionally enables only the BMA456 INT1 line on the shared
 * EXTI9_5 vector. RFM_DIO2, PEDO_INT2 and MPU_INT are masked during this
 * diagnostic stage so the test answers one question only: can PC7/BMA INT1
 * interrupt the MCU repeatedly without destabilising the baseline firmware?
 *
 * No SPI/I2C access and no event processing occurs in the ISR.
 */
void BmaIrqDiag_EnableCounterOnly(void);
void BmaIrqDiag_Disable(void);
bool BmaIrqDiag_IsEnabled(void);

/* Non-static on purpose for STM32CubeIDE Live Expressions. */
extern volatile uint32_t bma_irq_diag_count;
extern volatile uint32_t bma_irq_diag_unexpected_count;
extern volatile uint8_t bma_irq_diag_enabled;

#endif /* BMA_IRQ_DIAG_H */
