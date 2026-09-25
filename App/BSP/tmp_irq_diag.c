#include "tmp_irq_diag.h"
#include "main.h"
volatile uint32_t tmp_irq_diag_count=0U;
volatile bool tmp_irq_diag_enabled=false;
void TmpIrqDiag_EnableCounterOnly(void){GPIO_InitTypeDef g={0};g.Pin=TMP_INT_Pin;g.Mode=GPIO_MODE_IT_RISING;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(TMP_INT_GPIO_Port,&g);__HAL_GPIO_EXTI_CLEAR_IT(TMP_INT_Pin);HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);HAL_NVIC_SetPriority(EXTI15_10_IRQn,6U,0U);HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);tmp_irq_diag_enabled=true;}
void TmpIrqDiag_Disable(void){GPIO_InitTypeDef g={0};tmp_irq_diag_enabled=false;__HAL_GPIO_EXTI_CLEAR_IT(TMP_INT_Pin);g.Pin=TMP_INT_Pin;g.Mode=GPIO_MODE_INPUT;g.Pull=GPIO_NOPULL;g.Speed=GPIO_SPEED_FREQ_LOW;HAL_GPIO_Init(TMP_INT_GPIO_Port,&g);}
void TmpIrqDiag_OnGpioExti(uint16_t p){if(tmp_irq_diag_enabled&&p==TMP_INT_Pin)tmp_irq_diag_count++;}
