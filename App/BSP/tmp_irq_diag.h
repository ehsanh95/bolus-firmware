#ifndef TMP_IRQ_DIAG_H
#define TMP_IRQ_DIAG_H
#include <stdbool.h>
#include <stdint.h>
extern volatile uint32_t tmp_irq_diag_count;
extern volatile bool tmp_irq_diag_enabled;
void TmpIrqDiag_EnableCounterOnly(void);
void TmpIrqDiag_Disable(void);
void TmpIrqDiag_OnGpioExti(uint16_t gpio_pin);
#endif
