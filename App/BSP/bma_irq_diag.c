#include "bma_irq_diag.h"

#include "main.h"

volatile uint32_t bma_irq_diag_count = 0U;
volatile uint32_t bma_irq_diag_unexpected_count = 0U;
volatile uint8_t bma_irq_diag_enabled = 0U;

static uint32_t SharedUnexpectedMask(void)
{
    return (uint32_t)(RFM_DIO2_Pin | PEDO_INT2_Pin | MPU_INT_Pin);
}

void BmaIrqDiag_EnableCounterOnly(void)
{
    uint32_t unexpected_mask = SharedUnexpectedMask();

    /* Configure the shared vector from a known quiet state. */
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    /*
     * STEP-2 isolation: only PC7/BMA INT1 is allowed to request EXTI9_5.
     * The other currently configured EXTI lines sharing this vector are
     * deliberately masked until their ownership is reintroduced in a later
     * staged test.
     */
    EXTI->IMR1 &= ~unexpected_mask;
    EXTI->IMR1 |= (uint32_t)PEDO_INT1_Pin;

    __HAL_GPIO_EXTI_CLEAR_IT(RFM_DIO2_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(PEDO_INT2_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(MPU_INT_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(PEDO_INT1_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);

    bma_irq_diag_count = 0U;
    bma_irq_diag_unexpected_count = 0U;
    bma_irq_diag_enabled = 1U;

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

void BmaIrqDiag_Disable(void)
{
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    EXTI->IMR1 &= ~((uint32_t)PEDO_INT1_Pin);
    __HAL_GPIO_EXTI_CLEAR_IT(PEDO_INT1_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    bma_irq_diag_enabled = 0U;
}

bool BmaIrqDiag_IsEnabled(void)
{
    return (bma_irq_diag_enabled != 0U);
}

/*
 * Shared EXTI9_5 vector used only as a STEP-2 diagnostic harness.
 *
 * IMPORTANT: do not read BMA status, run HAL GPIO callbacks, touch SPI/I2C,
 * wake TMP/MPU, or execute EventProcessor from here. The only intended BMA
 * action is clearing the EXTI pending bit and incrementing one counter.
 */
void EXTI9_5_IRQHandler(void)
{
    uint32_t unexpected_mask;
    uint32_t unexpected_pending;

    if (__HAL_GPIO_EXTI_GET_IT(PEDO_INT1_Pin) != 0U)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(PEDO_INT1_Pin);
        bma_irq_diag_count++;
    }

    /* Defensive evidence only; these lines are masked for this test. */
    unexpected_mask = SharedUnexpectedMask();
    unexpected_pending = EXTI->PR1 & unexpected_mask;

    if (unexpected_pending != 0U)
    {
        EXTI->PR1 = unexpected_pending;
        bma_irq_diag_unexpected_count++;
    }
}
