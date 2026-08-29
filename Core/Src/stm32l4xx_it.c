/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * Diagnostic only. Production code must feed the watchdog from the application
 * health path, not blindly from SysTick. 100 ms is deliberately much shorter
 * than the current ~1 s IWDG timeout so we can prove whether watchdog starvation
 * is causing the observed reset loop while still allowing hard faults/disabled
 * interrupts to reset the MCU.
 */
#define BOLUS_IWDG_DIAG_SYSTICK_REFRESH_MS  100U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

volatile uint32_t iwdg_diag_systick_refresh_count = 0U;

/*
 * Shared EXTI9_5 diagnostics.
 *
 * EXTI line 5 belongs to RFM_DIO2 (PB5), line 6 to the currently unused
 * PEDO_INT2 (PC6), and line 7 to BMA456 PEDO_INT1 (PC7). They all share the
 * same NVIC vector. The previous bench handler serviced only lines 6/7, so a
 * pending radio DIO2 edge on line 5 could leave EXTI9_5 permanently pending
 * and starve lower-priority SysTick until IWDG reset the MCU.
 */
volatile uint32_t exti9_5_diag_entry_count = 0U;
volatile uint32_t rfm_dio2_diag_dispatch_count = 0U;
volatile uint32_t bma_irq_diag_entry_count = 0U;
volatile uint32_t bma_irq_diag_last_tick_ms = 0U;
volatile uint8_t bma_irq_diag_int1_masked = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

extern IWDG_HandleTypeDef hiwdg;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /*
   * TEMPORARY RESET-LOOP DIAGNOSTIC ONLY.
   *
   * If the board becomes stable with this enabled, the reset source is watchdog
   * starvation in the application/init path rather than sensor power-gate
   * settling itself. This must be removed once the offending blocking path is
   * identified; feeding IWDG unconditionally from SysTick is not an acceptable
   * production watchdog architecture.
   */
  if ((hiwdg.Instance == IWDG) &&
      ((HAL_GetTick() % BOLUS_IWDG_DIAG_SYSTICK_REFRESH_MS) == 0U))
  {
    (void)HAL_IWDG_Refresh(&hiwdg);
    iwdg_diag_systick_refresh_count++;
  }

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l4xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */

/*
 * EXTI9_5 is shared by three physical lines on the current PCB:
 *   line 5 -> PB5 / RFM_DIO2
 *   line 6 -> PC6 / PEDO_INT2 (unused for now)
 *   line 7 -> PC7 / PEDO_INT1 / BMA456 Any-Motion
 *
 * Every pending producer on a shared vector must be acknowledged. In the
 * previous diagnostic implementation the radio's line 5 was never cleared.
 * A single DIO2 edge could therefore retrigger this IRQ continuously and keep
 * the CPU away from SysTick until IWDG reset it.
 */
void EXTI9_5_IRQHandler(void)
{
  exti9_5_diag_entry_count++;

  if (__HAL_GPIO_EXTI_GET_IT(RFM_DIO2_Pin) != 0U)
  {
    rfm_dio2_diag_dispatch_count++;
    HAL_GPIO_EXTI_IRQHandler(RFM_DIO2_Pin);
  }

  /* INT2 has no owner in Phase 5; keep it masked and clear any stale edge. */
  EXTI->IMR1 &= ~((uint32_t)PEDO_INT2_Pin);
  if (__HAL_GPIO_EXTI_GET_IT(PEDO_INT2_Pin) != 0U)
  {
    __HAL_GPIO_EXTI_CLEAR_IT(PEDO_INT2_Pin);
  }

  if (__HAL_GPIO_EXTI_GET_IT(PEDO_INT1_Pin) != 0U)
  {
    bma_irq_diag_entry_count++;
    bma_irq_diag_last_tick_ms = HAL_GetTick();
    bma_irq_diag_int1_masked = 0U;
    HAL_GPIO_EXTI_IRQHandler(PEDO_INT1_Pin);
  }
}

/* USER CODE END 1 */
