/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bolus_power.h"
#include "fault_manager.h"
#include "bolus_led.h"
#include "battery.h"
#include "sensor_service.h"
#include "bma_event_service.h"
#include "bma_irq_diag.h"
#include "bolus_runtime_config.h"
#include "rfm95w_board.h"
#include "timer.h"
#include "radio.h"
#include "sx1276.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;
IWDG_HandleTypeDef hiwdg;
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint32_t reset_flags = 0U;
battery_status_t battery_status;

/* RFM95W / SX1276 Phase 4 regression diagnostics. */
static RadioEvents_t rfm95w_radio_events = {0};
uint32_t rfm95w_wakeup_time_ms = 0U;
uint8_t rfm95w_version_after_init = 0U;
uint8_t rfm95w_opmode_sleep = 0U;
uint8_t rfm95w_opmode_stby = 0U;
uint8_t rfm95w_frf_msb = 0U;
uint8_t rfm95w_frf_mid = 0U;
uint8_t rfm95w_frf_lsb = 0U;
uint32_t rfm95w_frf_actual = 0U;
uint32_t rfm95w_frf_expected = 0U;
RadioState_t rfm95w_state_after_init = RF_IDLE;
HAL_StatusTypeDef rfm95w_final_spi_status = HAL_ERROR;
bool rfm95w_init_ok = false;
bool rfm95w_sleep_ok = false;
bool rfm95w_stby_ok = false;
bool rfm95w_freq_ok = false;
bool rfm95w_final_ok = false;

/* BMA456 raw SPI Phase 4 regression diagnostics. */
uint8_t bma456_first_read = 0U;
uint8_t bma456_chip_id = 0U;
HAL_StatusTypeDef bma456_spi_status = HAL_ERROR;
bool bma456_chip_id_ok = false;
HAL_StatusTypeDef bma456_pwr_status = HAL_ERROR;
uint8_t bma456_pwr_conf_1 = 0U;
uint8_t bma456_pwr_conf_2 = 0U;
uint8_t bma456_chip_id_3 = 0U;

/* Shared Phase 5 runtime configuration used by the bench services. */
bolus_runtime_config_t sensor_service_config = {0};

/* BMA456 SensorService diagnostics. */
sensor_service_status_t bma456_service_init_status = SENSOR_SERVICE_ERROR_BMA_INIT;
sensor_service_status_t bma456_service_read_status = SENSOR_SERVICE_ERROR_BMA_READ;
sensor_service_bma_sample_t bma456_service_sample = {0};
bool bma456_service_ready = false;
uint32_t bma456_service_last_read_tick = 0U;

/* BMA Any-Motion + isolated INT1 bring-up diagnostics. */
bma_event_service_status_t bma_event_service_init_status = BMA_EVENT_SERVICE_ERROR_INIT;
bma_event_service_status_t bma_event_service_read_status = BMA_EVENT_SERVICE_ERROR_READ;
bma_event_service_sample_t bma_event_service_sample = {0};
bool bma_event_service_ready = false;
uint32_t bma_event_service_processed_irq_count = 0U;
uint32_t bma_event_service_ack_count = 0U;
uint32_t bma_event_service_any_motion_count = 0U;

/* TMP117 SensorService diagnostics. */
sensor_service_status_t tmp_service_init_status = SENSOR_SERVICE_ERROR_TMP_INIT;
sensor_service_status_t tmp_service_read_status = SENSOR_SERVICE_ERROR_TMP_READ;
sensor_service_temperature_sample_t tmp_service_sample = {0};
bool tmp_service_ready = false;
uint32_t tmp_service_last_read_tick = 0U;

/* MPU6050 SensorService diagnostics. */
sensor_service_status_t mpu_service_init_status = SENSOR_SERVICE_ERROR_MPU_INIT;
sensor_service_status_t mpu_service_read_status = SENSOR_SERVICE_ERROR_MPU_READ;
sensor_service_mpu_sample_t mpu_service_sample = {0};
bool mpu_service_ready = false;
uint32_t mpu_service_last_read_tick = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef BMA456_RawReadRegister(uint8_t reg, uint8_t *value)
{
    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};
    HAL_StatusTypeDef status;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    tx[0] = reg | 0x80U;

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3U, 20U);
    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    if (status == HAL_OK)
    {
        *value = rx[2];
    }

    return status;
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  reset_flags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
#ifdef DEBUG
  __HAL_DBGMCU_FREEZE_IWDG();
#endif
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_ADC1_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();

  /* USER CODE BEGIN 2 */
  BolusPower_Init();
  BolusLed_Init();
  FaultManager_Init();

  battery_status = Battery_Init(&hadc1);
  if (battery_status == BATTERY_OK)
  {
      BolusLed_On(BOLUS_LED_SENSOR);
      HAL_Delay(150U);
      BolusLed_Off(BOLUS_LED_SENSOR);
  }

  /* Keep the known-good Phase 4 BMA raw SPI sanity check during migration. */
  BolusPower_On(BOLUS_POWER_BMA456);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);
  HAL_Delay(1U);
  (void)BMA456_RawReadRegister(0x00U, &bma456_first_read);
  HAL_Delay(1U);
  bma456_spi_status = BMA456_RawReadRegister(0x00U, &bma456_chip_id);
  bma456_chip_id_ok = ((bma456_spi_status == HAL_OK) && (bma456_chip_id == 0x16U));
  bma456_pwr_status = BMA456_RawReadRegister(0x7CU, &bma456_pwr_conf_1);
  HAL_Delay(1U);
  (void)BMA456_RawReadRegister(0x7CU, &bma456_pwr_conf_2);
  HAL_Delay(1U);
  (void)BMA456_RawReadRegister(0x00U, &bma456_chip_id_3);

  BolusRuntimeConfig_LoadDefaults(&sensor_service_config);

  /* BMA456: continuous low-power sentinel. */
  bma456_service_init_status = SensorService_InitBma(&hspi2, &sensor_service_config);
  bma456_service_ready =
      ((bma456_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsBmaReady());
  if (bma456_service_ready)
  {
      bma456_service_read_status = SensorService_ReadBmaSample(&bma456_service_sample);
      bma456_service_last_read_tick = HAL_GetTick();

      /*
       * Staged bring-up STEP 3:
       * configure/read back BMA456 Any-Motion, enable only PC7/BMA INT1 at
       * the MCU, then acknowledge feature status later from the main context.
       * No SPI/I2C work is performed in the EXTI ISR.
       */
      bma_event_service_init_status =
          BmaEventService_Init(&hspi2, &sensor_service_config);
      bma_event_service_ready =
          ((bma_event_service_init_status == BMA_EVENT_SERVICE_OK) &&
           BmaEventService_IsReady());
  }

  /* TMP117: one-shot temperature path, shutdown between samples. */
  tmp_service_init_status = SensorService_InitTemperature(&hi2c3, &sensor_service_config);
  tmp_service_ready =
      ((tmp_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsTemperatureReady());
  if (tmp_service_ready)
  {
      tmp_service_read_status = SensorService_ReadTemperatureOneShot(&tmp_service_sample);
      tmp_service_last_read_tick = HAL_GetTick();
  }

  /*
   * MPU6050: normally rail-OFF. Init verifies the path and powers it back OFF;
   * each sample below is a complete power-up/configure/read/sleep/power-off
   * transaction owned exclusively by SensorService.
   */
  mpu_service_init_status = SensorService_InitMpu(&hi2c1, &sensor_service_config);
  mpu_service_ready =
      ((mpu_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsMpuReady());
  if (mpu_service_ready)
  {
      mpu_service_read_status = SensorService_ReadMpuSample(&mpu_service_sample);
      mpu_service_last_read_tick = HAL_GetTick();
  }

  /* RFM95W / SX1276 Phase 4 regression test. No RF transmission. */
  BolusPower_On(BOLUS_POWER_RFM95W);
  HAL_Delay(RFM95W_POWERUP_DELAY_MS);
  Sx_Board_Bus_Init();
  Sx_Board_IoInit();
  HAL_IWDG_Refresh(&hiwdg);
  rfm95w_wakeup_time_ms = SX1276Init(&rfm95w_radio_events);
  HAL_IWDG_Refresh(&hiwdg);
  rfm95w_version_after_init = SX1276Read(RFM95W_REG_VERSION);
  rfm95w_state_after_init = SX1276GetStatus();
  rfm95w_init_ok =
      ((rfm95w_version_after_init == RFM95W_EXPECTED_VERSION) &&
       (rfm95w_state_after_init == RF_IDLE));

  SX1276SetModem(MODEM_LORA);
  SX1276SetSleep();
  HAL_Delay(2U);
  rfm95w_opmode_sleep = SX1276Read(REG_OPMODE);
  rfm95w_sleep_ok =
      (((rfm95w_opmode_sleep & 0x80U) != 0U) &&
       ((rfm95w_opmode_sleep & 0x07U) == RF_OPMODE_SLEEP));

  SX1276SetStby();
  HAL_Delay(2U);
  rfm95w_opmode_stby = SX1276Read(REG_OPMODE);
  rfm95w_stby_ok =
      (((rfm95w_opmode_stby & 0x80U) != 0U) &&
       ((rfm95w_opmode_stby & 0x07U) == RF_OPMODE_STANDBY));

  SX1276SetChannel(RFM95W_DEFAULT_FREQUENCY_HZ);
  rfm95w_frf_msb = SX1276Read(REG_FRFMSB);
  rfm95w_frf_mid = SX1276Read(REG_FRFMID);
  rfm95w_frf_lsb = SX1276Read(REG_FRFLSB);
  rfm95w_frf_actual =
      ((uint32_t)rfm95w_frf_msb << 16) |
      ((uint32_t)rfm95w_frf_mid << 8) |
      ((uint32_t)rfm95w_frf_lsb);
  rfm95w_frf_expected =
      (uint32_t)((((uint64_t)RFM95W_DEFAULT_FREQUENCY_HZ) << 19) / 32000000ULL);
  rfm95w_freq_ok = (rfm95w_frf_actual == rfm95w_frf_expected);
  rfm95w_final_spi_status = RFM95W_Board_GetLastSpiStatus();
  rfm95w_final_ok =
      (rfm95w_init_ok && rfm95w_sleep_ok && rfm95w_stby_ok && rfm95w_freq_ok &&
       (rfm95w_final_spi_status == HAL_OK));
  SX1276SetSleep();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    TimerProcess();

    /*
     * Consume BMA INT1 edges in normal main context. The EXTI ISR only counts
     * edges; the SPI status read happens here so the Bosch feature interrupt
     * is acknowledged/re-armed without doing bus work inside the ISR.
     */
    if (bma_event_service_ready)
    {
        uint32_t irq_count_snapshot = bma_irq_diag_count;

        if (irq_count_snapshot != bma_event_service_processed_irq_count)
        {
            bma_event_service_read_status =
                BmaEventService_Read(&bma_event_service_sample);

            if (bma_event_service_read_status == BMA_EVENT_SERVICE_OK)
            {
                bma_event_service_processed_irq_count = irq_count_snapshot;
                bma_event_service_ack_count++;

                if (bma_event_service_sample.any_motion)
                {
                    bma_event_service_any_motion_count++;
                }
            }
        }
    }

    if (bma456_service_ready &&
        ((HAL_GetTick() - bma456_service_last_read_tick) >= 500U))
    {
        bma456_service_last_read_tick = HAL_GetTick();
        bma456_service_read_status = SensorService_ReadBmaSample(&bma456_service_sample);
    }

    if (tmp_service_ready &&
        ((HAL_GetTick() - tmp_service_last_read_tick) >= 2000U))
    {
        tmp_service_last_read_tick = HAL_GetTick();
        tmp_service_read_status = SensorService_ReadTemperatureOneShot(&tmp_service_sample);
    }

    /* Bench-only 5 s cadence. Production MPU acquisition is event/schedule driven. */
    if (mpu_service_ready &&
        ((HAL_GetTick() - mpu_service_last_read_tick) >= 5000U))
    {
        mpu_service_last_read_tick = HAL_GetTick();
        mpu_service_read_status = SensorService_ReadMpuSample(&mpu_service_sample);
    }

    HAL_IWDG_Refresh(&hiwdg);
    HAL_Delay(10U);
  }
  /* USER CODE END 3 */
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_ADC1_Init(void)
{
  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_14;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x10909CEC;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_I2C3_Init(void)
{
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10909CEC;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_IWDG_Init(void)
{
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 999;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI2_Init(void)
{
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  HAL_GPIO_WritePin(GPIOA, RFM_RST_Pin|LED3_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, RFM95W_NSS_Pin|MPU_PWR_ON_Pin|MCU_BCK_PWR_ON_Pin|SOC_CHK_ON_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, RFM_PWR_ON_Pin|PEDO_PWR_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOB, Pedo_NSS_Pin|TMP_PWR_ON_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOC, Main_Reg_PWR_ON_Pin|LED1_Pin, GPIO_PIN_RESET);

  /* Phase-5 sensor-event bring-up: BMA INT1 is the only active EXTI source. */
  GPIO_InitStruct.Pin = PEDO_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PEDO_INT1_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = TMP_INT_Pin|PEDO_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RFM_RST_Pin|LED3_Pin|RFM95W_NSS_Pin|MPU_PWR_ON_Pin
                          |MCU_BCK_PWR_ON_Pin|SOC_CHK_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RFM_PWR_ON_Pin|Main_Reg_PWR_ON_Pin|PEDO_PWR_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED2_Pin|Pedo_NSS_Pin|TMP_PWR_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_GPIO_Port, &GPIO_InitStruct);

  /* Radio DIOs stay readable inputs; their EXTI paths are not enabled yet. */
  GPIO_InitStruct.Pin = RFM_DIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RFM_DIO0_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RFM_DIO1_Pin|RFM_DIO2_Pin|MPU_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */