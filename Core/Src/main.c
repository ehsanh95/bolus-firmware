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
#include "tmp117.h"
#include "MPU6050.h"
#include "sensor_service.h"
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

/*
 * ============================================================
 * Existing application diagnostics/state
 * ============================================================
 */

uint32_t reset_flags = 0U;

battery_status_t battery_status;

GPIO_PinState last_button_state = GPIO_PIN_SET;

/*
 * ============================================================
 * RFM95W / SX1276 Phase 4 final validation
 * ============================================================
 */

static RadioEvents_t rfm95w_radio_events = {0};

uint32_t rfm95w_wakeup_time_ms = 0U;

uint8_t rfm95w_version_after_init = 0U;

uint8_t rfm95w_opmode_sleep = 0U;
uint8_t rfm95w_opmode_stby  = 0U;

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


/* ============================================================
 * TMP117 legacy Phase 4 debug variables
 * ============================================================ */

bool tmp117_active = false;

uint8_t tmp117_buffer[3] = {0};

uint16_t tmp117_device_id = 0U;

double tmp117_temperature_c = 0.0;

bool tmp117_init_ok = false;

bool tmp117_read_ok = false;

bool tmp117_shutdown_ok = false;

uint32_t tmp117_last_read_ms = 0U;


/* ============================================================
 * MPU6050 debug variables
 * ============================================================ */

MPU6050_t mpu_data = {0};

bool mpu_active = false;

bool mpu_bus_ok = false;

uint8_t mpu_init_status = 1U;

uint32_t mpu_last_read_ms = 0U;


/* ============================================================
 * BMA456 raw SPI bring-up variables
 * ============================================================ */

uint8_t bma456_first_read = 0U;

uint8_t bma456_chip_id = 0U;

HAL_StatusTypeDef bma456_spi_status = HAL_ERROR;

bool bma456_chip_id_ok = false;

uint8_t bma456_pwr_conf = 0U;

HAL_StatusTypeDef bma456_pwr_status = HAL_ERROR;

uint8_t bma456_pwr_conf_1 = 0U;

uint8_t bma456_pwr_conf_2 = 0U;

uint8_t bma456_chip_id_3 = 0U;

/*
 * ============================================================
 * Phase 5 BMA456 SensorService bench variables
 * ============================================================
 *
 * Keep these non-static while the Phase 5 BMA path is under bench validation
 * so they are easy to inspect in CubeIDE Live Expressions.
 */
bolus_runtime_config_t bma456_service_config = {0};

sensor_service_status_t bma456_service_init_status =
    SENSOR_SERVICE_ERROR_BMA_INIT;

sensor_service_status_t bma456_service_read_status =
    SENSOR_SERVICE_ERROR_BMA_READ;

sensor_service_bma_sample_t bma456_service_sample = {0};

bool bma456_service_ready = false;

uint32_t bma456_service_last_read_tick = 0U;

/*
 * ============================================================
 * Phase 5 TMP117 SensorService bench variables
 * ============================================================
 *
 * The active TMP117 path is now SensorService-owned. The old button-driven
 * direct-driver path below is compiled out during this bench migration.
 */
sensor_service_status_t tmp_service_init_status =
    SENSOR_SERVICE_ERROR_TMP_INIT;

sensor_service_status_t tmp_service_read_status =
    SENSOR_SERVICE_ERROR_TMP_READ;

sensor_service_temperature_sample_t tmp_service_sample = {0};

bool tmp_service_ready = false;

uint32_t tmp_service_last_read_tick = 0U;

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

static HAL_StatusTypeDef BMA456_RawReadRegister(
    uint8_t reg,
    uint8_t *value)
{
    uint8_t tx[3] = {0};
    uint8_t rx[3] = {0};

    HAL_StatusTypeDef status;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * BMA456 SPI read
     *
     * Byte 0:
     *   Read command + register address
     *
     * Byte 1:
     *   Dummy clock
     *
     * Byte 2:
     *   Clock actual register data
     */
    tx[0] = reg | 0x80U;
    tx[1] = 0x00U;
    tx[2] = 0x00U;


    /*
     * CS active LOW
     */
    HAL_GPIO_WritePin(
        Pedo_NSS_GPIO_Port,
        Pedo_NSS_Pin,
        GPIO_PIN_RESET);


    status =
        HAL_SPI_TransmitReceive(
            &hspi2,
            tx,
            rx,
            3U,
            20U);


    /*
     * CS inactive HIGH
     */
    HAL_GPIO_WritePin(
        Pedo_NSS_GPIO_Port,
        Pedo_NSS_Pin,
        GPIO_PIN_SET);


    if (status == HAL_OK)
    {
        /*
         * rx[0] = undefined during command
         * rx[1] = dummy
         * rx[2] = actual register value
         */
        *value = rx[2];
    }

    return status;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  reset_flags = RCC->CSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();

  /*
   * ============================================================
   * BOLUS DEBUG SUPPORT
   * ============================================================
   * Freeze IWDG while CPU is halted by the debugger.
   *
   * This affects DEBUG sessions only.
   * During normal RUN execution IWDG remains active.
   */
  #ifdef DEBUG
  __HAL_DBGMCU_FREEZE_IWDG();
  #endif

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_ADC1_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */


  /* ============================================================
   * Bolus subsystem initialization
   * ============================================================ */

  BolusPower_Init();

  BolusLed_Init();

  FaultManager_Init();




  /* ============================================================
   * Battery driver initialization
   * ============================================================ */

  battery_status =
      Battery_Init(&hadc1);


  /*
   * One blink means Battery driver initialized.
   */
  if (battery_status == BATTERY_OK)
  {
      BolusLed_On(
          BOLUS_LED_SENSOR);

      HAL_Delay(150);

      BolusLed_Off(
          BOLUS_LED_SENSOR);
  }


  /* ============================================================
   * BMA456 RAW SPI BRING-UP TEST
   * ============================================================
   *
   * Goal:
   *
   * Power ON
   *      ↓
   * First dummy CHIP_ID read
   *      ↓
   * Second CHIP_ID read
   *      ↓
   * Read PWR_CONF
   *
   * This remains temporarily as a Phase 4 hardware sanity check before
   * ownership passes to SensorService for the Phase 5 bench path below.
   * ============================================================
   */


  /*
   * Physical power ON.
   */
  BolusPower_On(
      BOLUS_POWER_BMA456);


  /*
   * Allow BMA456 supply/startup to stabilize.
   */
  HAL_Delay(10);


  /*
   * Make sure CS is inactive before transaction.
   */
  HAL_GPIO_WritePin(
      Pedo_NSS_GPIO_Port,
      Pedo_NSS_Pin,
      GPIO_PIN_SET);

  HAL_Delay(1);


  /*
   * ------------------------------------------------------------
   * First CHIP_ID read
   * ------------------------------------------------------------
   *
   * This first SPI transaction is intentionally discarded.
   */
  (void)BMA456_RawReadRegister(
      0x00U,
      &bma456_first_read);


  HAL_Delay(1);


  /*
   * ------------------------------------------------------------
   * Second CHIP_ID read
   * ------------------------------------------------------------
   *
   * Register:
   * 0x00 = CHIP_ID
   *
   * Expected BMA456 CHIP_ID:
   * 0x16 = decimal 22
   */
  bma456_spi_status =
      BMA456_RawReadRegister(
          0x00U,
          &bma456_chip_id);


  /*
   * Communication PASS condition.
   */
  bma456_chip_id_ok =
      ((bma456_spi_status == HAL_OK) &&
       (bma456_chip_id == 0x16U));


  /*
   * ============================================================
   * BMA456 SPI sequencing diagnostic
   * ============================================================
   *
   * Goal:
   *
   * CHIP_ID       -> already read above
   * PWR_CONF #1   -> register 0x7C
   * PWR_CONF #2   -> register 0x7C again
   * CHIP_ID #3    -> register 0x00 again
   *
   * This lets us detect whether SPI data is delayed
   * by one transaction.
   * ============================================================
   */


  /*
   * First PWR_CONF read
   */
  bma456_pwr_status =
      BMA456_RawReadRegister(
          0x7CU,
          &bma456_pwr_conf_1);


  HAL_Delay(1);


  /*
   * Second PWR_CONF read
   */
  (void)BMA456_RawReadRegister(
      0x7CU,
      &bma456_pwr_conf_2);


  HAL_Delay(1);


  /*
   * Read CHIP_ID again
   */
  (void)BMA456_RawReadRegister(
      0x00U,
      &bma456_chip_id_3);

  /*
   * ============================================================
   * Phase 5 BMA456 SensorService bench initialization
   * ============================================================
   *
   * The legacy BMA456_Bolus_* Step Counter path is intentionally NOT started.
   * SensorService now owns the active BMA456 software path so one Bosch device
   * instance is responsible for XYZ + Step Counter data.
   */
  BolusRuntimeConfig_LoadDefaults(
      &bma456_service_config);

  bma456_service_init_status =
      SensorService_InitBma(
          &hspi2,
          &bma456_service_config);

  bma456_service_ready =
      ((bma456_service_init_status == SENSOR_SERVICE_OK) &&
       SensorService_IsBmaReady());

  if (bma456_service_ready)
  {
      /*
       * First successful sample establishes the Step Counter baseline, so
       * step_delta is expected to be 0 for this first acquisition.
       */
      bma456_service_read_status =
          SensorService_ReadBmaSample(
              &bma456_service_sample);
  }

  /*
   * ============================================================
   * Phase 5 TMP117 SensorService bench initialization
   * ============================================================
   *
   * This is now the only active TMP117 path. The service powers the sensor,
   * verifies Device ID, configures AVG1, leaves the device in shutdown, then
   * performs a bounded one-shot conversion for the first sample.
   */
  tmp_service_init_status =
      SensorService_InitTemperature(
          &hi2c3,
          &bma456_service_config);

  tmp_service_ready =
      ((tmp_service_init_status == SENSOR_SERVICE_OK) &&
       SensorService_IsTemperatureReady());

  if (tmp_service_ready)
  {
      tmp_service_read_status =
          SensorService_ReadTemperatureOneShot(
              &tmp_service_sample);

      tmp_service_last_read_tick = HAL_GetTick();
  }

  /*
   * ============================================================
   * RFM95W / SX1276 Phase 4 FINAL TEST
   * ============================================================
   *
   * This test performs no RF transmission.
   *
   * Validates:
   *  - Power
   *  - Full SX1276 initialization
   *  - SPI communication
   *  - Sleep mode
   *  - Standby mode
   *  - Frequency programming
   * ============================================================
   */


  /* ------------------------------------------------------------
   * 1. Power ON RFM95W
   * ------------------------------------------------------------
   */

  BolusPower_On(
      BOLUS_POWER_RFM95W);

  HAL_Delay(
      RFM95W_POWERUP_DELAY_MS);


  /* ------------------------------------------------------------
   * 2. Prepare SPI / NSS
   * ------------------------------------------------------------
   */

  Sx_Board_Bus_Init();

  Sx_Board_IoInit();


  /* ------------------------------------------------------------
   * 3. Full SX1276 initialization
   * ------------------------------------------------------------
   */

  HAL_IWDG_Refresh(
      &hiwdg);

  rfm95w_wakeup_time_ms =
      SX1276Init(
          &rfm95w_radio_events);

  HAL_IWDG_Refresh(
      &hiwdg);


  /* ------------------------------------------------------------
   * 4. Verify RegVersion after full init
   * ------------------------------------------------------------
   */

  rfm95w_version_after_init =
      SX1276Read(
          RFM95W_REG_VERSION);

  rfm95w_state_after_init =
      SX1276GetStatus();


  rfm95w_init_ok =
      ((rfm95w_version_after_init ==
        RFM95W_EXPECTED_VERSION) &&
       (rfm95w_state_after_init ==
        RF_IDLE));


  /* ------------------------------------------------------------
   * 5. Select LoRa modem
   * ------------------------------------------------------------
   */

  SX1276SetModem(
      MODEM_LORA);


  /* ------------------------------------------------------------
   * 6. Sleep test
   * ------------------------------------------------------------
   */

  SX1276SetSleep();

  HAL_Delay(2U);

  rfm95w_opmode_sleep =
      SX1276Read(
          REG_OPMODE);


  rfm95w_sleep_ok =
      (((rfm95w_opmode_sleep & 0x80U) != 0U) &&
       ((rfm95w_opmode_sleep & 0x07U) ==
        RF_OPMODE_SLEEP));


  /* ------------------------------------------------------------
   * 7. Standby test
   * ------------------------------------------------------------
   */

  SX1276SetStby();

  HAL_Delay(2U);

  rfm95w_opmode_stby =
      SX1276Read(
          REG_OPMODE);


  rfm95w_stby_ok =
      (((rfm95w_opmode_stby & 0x80U) != 0U) &&
       ((rfm95w_opmode_stby & 0x07U) ==
        RF_OPMODE_STANDBY));


  /* ------------------------------------------------------------
   * 8. Program 868 MHz
   * ------------------------------------------------------------
   */

  SX1276SetChannel(
      RFM95W_DEFAULT_FREQUENCY_HZ);


  /* Read FRF registers back */
  rfm95w_frf_msb =
      SX1276Read(
          REG_FRFMSB);

  rfm95w_frf_mid =
      SX1276Read(
          REG_FRFMID);

  rfm95w_frf_lsb =
      SX1276Read(
          REG_FRFLSB);


  /* Reconstruct 24-bit FRF value */
  rfm95w_frf_actual =
      ((uint32_t)rfm95w_frf_msb << 16) |
      ((uint32_t)rfm95w_frf_mid << 8)  |
      ((uint32_t)rfm95w_frf_lsb);


  /*
   * SX1276:
   *
   * FRF = Frequency * 2^19 / 32 MHz
   */
  rfm95w_frf_expected =
      (uint32_t)
      ((((uint64_t)
         RFM95W_DEFAULT_FREQUENCY_HZ)
        << 19) /
       32000000ULL);


  rfm95w_freq_ok =
      (rfm95w_frf_actual ==
       rfm95w_frf_expected);


  /* ------------------------------------------------------------
   * 9. SPI diagnostic
   * ------------------------------------------------------------
   */

  rfm95w_final_spi_status =
      RFM95W_Board_GetLastSpiStatus();


  /* ------------------------------------------------------------
   * 10. FINAL RESULT
   * ------------------------------------------------------------
   */

  rfm95w_final_ok =
      (rfm95w_init_ok &&
       rfm95w_sleep_ok &&
       rfm95w_stby_ok &&
       rfm95w_freq_ok &&
       (rfm95w_final_spi_status ==
        HAL_OK));


  /*
   * Leave radio in low-power state.
   */
  SX1276SetSleep();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  /*
	   * ============================================================
	   * SX1276 cooperative timer service
	   * ============================================================
	   */
	  TimerProcess();

	  /*
	   * ------------------------------------------------------------
	   * Phase 5 BMA456 SensorService bench polling
	   * ------------------------------------------------------------
	   *
	   * 500 ms is intentionally a bench/debug interval only. Production
	   * acquisition will be event/RTC driven so the MCU can remain in STOP2.
	   */
	  if (bma456_service_ready)
	  {
	      if ((HAL_GetTick() - bma456_service_last_read_tick) >= 500U)
	      {
	          bma456_service_last_read_tick = HAL_GetTick();

	          bma456_service_read_status =
	              SensorService_ReadBmaSample(
	                  &bma456_service_sample);
	      }
	  }

	  /*
	   * ------------------------------------------------------------
	   * Phase 5 TMP117 SensorService bench polling
	   * ------------------------------------------------------------
	   *
	   * 2 s is a bench-only interval. Each call still performs exactly one
	   * conversion and returns the TMP117 to shutdown before the next interval.
	   */
	  if (tmp_service_ready)
	  {
	      if ((HAL_GetTick() - tmp_service_last_read_tick) >= 2000U)
	      {
	          tmp_service_last_read_tick = HAL_GetTick();

	          tmp_service_read_status =
	              SensorService_ReadTemperatureOneShot(
	                  &tmp_service_sample);
	      }
	  }

	  GPIO_PinState button_state =
	      HAL_GPIO_ReadPin(
	          BUTTON_GPIO_Port,
	          BUTTON_Pin);


	  /* ============================================================
	   * Button press detection
	   *
	   * Released = HIGH
	   * Pressed  = LOW
	   * ============================================================ */

	  if ((last_button_state == GPIO_PIN_SET) &&
	      (button_state == GPIO_PIN_RESET))
	  {
	      HAL_Delay(30);


	      /*
	       * Debounce confirmation
	       */
	      if (HAL_GPIO_ReadPin(
	              BUTTON_GPIO_Port,
	              BUTTON_Pin) == GPIO_PIN_RESET)
	      {
	          /*
	           * ====================================================
	           * FIRST PRESS
	           *
	           * Power ON + Init MPU6050. TMP117 is now owned by
	           * SensorService and is intentionally excluded here.
	           * ====================================================
	           */

	          if ((!mpu_active) &&
	              (!tmp117_active))
	          {
	              /*
	               * ================================================
	               * MPU6050
	               * ================================================
	               */

	              mpu_init_status = 1U;

	              mpu_bus_ok = false;


	              /*
	               * Physical power ON
	               */
	              BolusPower_On(
	                  BOLUS_POWER_MPU6050);


	              HAL_Delay(10);


	              /*
	               * Driver init
	               *
	               * 0 = success
	               * 1 = failure
	               */
	              mpu_init_status =
	                  MPU6050_Init(
	                      &hi2c1);


	              if (mpu_init_status == 0U)
	              {
	                  /*
	                   * Init may leave device in sleep
	                   * because SLEEP_BETWEEN_SAMPLES is enabled.
	                   */
	                  MPU6050_WakeAndStabilize(
	                      &hi2c1);


	                  /*
	                   * Confirm I2C communication.
	                   */
	                  mpu_bus_ok =
	                      (HAL_I2C_IsDeviceReady(
	                          &hi2c1,
	                          MPU6050_I2C_ADDRESS,
	                          2U,
	                          MPU6050_I2C_TIMEOUT_MS)
	                       == HAL_OK);


	                  if (mpu_bus_ok)
	                  {
	                      /*
	                       * First sample.
	                       */
	                      MPU6050_Read_All(
	                          &hi2c1,
	                          &mpu_data);


	                      mpu_active = true;


	                      mpu_last_read_ms =
	                          HAL_GetTick();
	                  }
	              }


	              /*
	               * MPU6050 startup failure cleanup
	               */
	              if (!mpu_active)
	              {
	                  MPU6050_Sleep(
	                      &hi2c1);


	                  HAL_Delay(5);


	                  BolusPower_Off(
	                      BOLUS_POWER_MPU6050);
	              }

#if 0
	              /*
	               * ============================================================
	               * Legacy Phase 4 TMP117 direct-driver bench path
	               * ============================================================
	               * Disabled during Phase 5 migration. SensorService is the
	               * only active TMP117 owner; this block is kept temporarily as
	               * bring-up history until the migration is accepted on bench.
	               */

	              tmp117_init_ok = false;

	              tmp117_read_ok = false;

	              tmp117_shutdown_ok = false;


	              /*
	               * Physical power ON
	               */
	              BolusPower_On(
	                  BOLUS_POWER_TMP117);


	              HAL_Delay(10);


	              /*
	               * Driver init
	               */
	              tmp117_init_ok =
	                  TMP117_Init(
	                      &hi2c3,
	                      tmp117_buffer);


	              /*
	               * Read Device ID
	               */
	              if (tmp117_init_ok)
	              {
	                  tmp117_init_ok =
	                      TMP117_getDeviceID(
	                          &hi2c3,
	                          tmp117_buffer,
	                          &tmp117_device_id);
	              }


	              /*
	               * Continuous conversion for Live Debug
	               */
	              if (tmp117_init_ok)
	              {
	                  tmp117_init_ok =
	                      TMP117_setConversionMode(
	                          &hi2c3,
	                          tmp117_buffer,
	                          TMP117_CC_MODE);
	              }


	              /*
	               * First temperature sample
	               */
	              if (tmp117_init_ok)
	              {
	                  HAL_Delay(20);


	                  tmp117_read_ok =
	                      TMP117_getResultTemperature(
	                          &hi2c3,
	                          tmp117_buffer,
	                          &tmp117_temperature_c);
	              }


	              /*
	               * TMP117 startup success
	               */
	              if (tmp117_init_ok &&
	                  tmp117_read_ok)
	              {
	                  tmp117_active = true;


	                  tmp117_last_read_ms =
	                      HAL_GetTick();
	              }
	              else
	              {
	                  /*
	                   * TMP117 startup failure cleanup
	                   */

	                  (void)TMP117_setConversionMode(
	                      &hi2c3,
	                      tmp117_buffer,
	                      TMP117_SD_MODE);


	                  HAL_Delay(5);


	                  BolusPower_Off(
	                      BOLUS_POWER_TMP117);


	                  tmp117_active = false;
	              }
#endif


	              /*
	               * At least one button-controlled sensor started successfully.
	               */
	              if (mpu_active ||
	                  tmp117_active)
	              {
	                  BolusLed_On(
	                      BOLUS_LED_SENSOR);


	                  HAL_Delay(200);


	                  BolusLed_Off(
	                      BOLUS_LED_SENSOR);
	              }
	              else
	              {
	                  /*
	                   * Button-controlled sensor startup failed.
	                   *
	                   * Two short blinks.
	                   */
	                  for (uint8_t i = 0U;
	                       i < 2U;
	                       i++)
	                  {
	                      BolusLed_On(
	                          BOLUS_LED_SENSOR);


	                      HAL_Delay(80);


	                      BolusLed_Off(
	                          BOLUS_LED_SENSOR);


	                      HAL_Delay(80);
	                  }
	              }
	          }


	          /*
	           * ====================================================
	           * SECOND PRESS
	           *
	           * Software sleep/shutdown
	           * then physical Power OFF
	           * ====================================================
	           */

	          else
	          {
	              /*
	               * ================================================
	               * MPU6050 OFF
	               * ================================================
	               */

	              if (mpu_active)
	              {
	                  MPU6050_Sleep(
	                      &hi2c1);


	                  HAL_Delay(5);


	                  BolusPower_Off(
	                      BOLUS_POWER_MPU6050);


	                  mpu_active = false;

	                  mpu_bus_ok = false;
	              }

#if 0
	              /*
	               * Legacy TMP117 OFF path disabled with the direct-driver
	               * startup path above. SensorService keeps TMP117 in shutdown
	               * between one-shot conversions.
	               */
	              if (tmp117_active)
	              {
	                  tmp117_shutdown_ok =
	                      TMP117_setConversionMode(
	                          &hi2c3,
	                          tmp117_buffer,
	                          TMP117_SD_MODE);


	                  HAL_Delay(5);


	                  BolusPower_Off(
	                      BOLUS_POWER_TMP117);


	                  tmp117_active = false;
	              }
#endif


	              /*
	               * OFF indication
	               */
	              BolusLed_On(
	                  BOLUS_LED_SENSOR);


	              HAL_Delay(80);


	              BolusLed_Off(
	                  BOLUS_LED_SENSOR);
	          }
	      }
	  }


	  /* ============================================================
	   * Live MPU6050 sampling
	   * ============================================================ */

	  if (mpu_active)
	  {
	      if ((HAL_GetTick() -
	           mpu_last_read_ms) >= 100U)
	      {
	          mpu_bus_ok =
	              (HAL_I2C_IsDeviceReady(
	                  &hi2c1,
	                  MPU6050_I2C_ADDRESS,
	                  1U,
	                  MPU6050_I2C_TIMEOUT_MS)
	               == HAL_OK);


	          if (mpu_bus_ok)
	          {
	              MPU6050_Read_All(
	                  &hi2c1,
	                  &mpu_data);
	          }


	          mpu_last_read_ms =
	              HAL_GetTick();
	      }
	  }

#if 0
	  /* ============================================================
	   * Legacy live TMP117 direct-driver sampling
	   * ============================================================
	   * Disabled: Phase 5 TMP117 acquisition is handled above by
	   * SensorService_ReadTemperatureOneShot().
	   */

	  if (tmp117_active)
	  {
	      if ((HAL_GetTick() -
	           tmp117_last_read_ms) >= 250U)
	      {
	          tmp117_read_ok =
	              TMP117_getResultTemperature(
	                  &hi2c3,
	                  tmp117_buffer,
	                  &tmp117_temperature_c);


	          tmp117_last_read_ms =
	              HAL_GetTick();
	      }
	  }
#endif


	  /* ============================================================
	   * Button edge state
	   * ============================================================ */

	  last_button_state =
	      button_state;


	  /* ============================================================
	   * Watchdog service
	   * ============================================================ */

	  HAL_IWDG_Refresh(
	      &hiwdg);


	  HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
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

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
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

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
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
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
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

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
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

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 999;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
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
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
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
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RFM_RST_Pin|LED3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, RFM95W_NSS_Pin|MPU_PWR_ON_Pin|MCU_BCK_PWR_ON_Pin|SOC_CHK_ON_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, RFM_PWR_ON_Pin|PEDO_PWR_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, Pedo_NSS_Pin|TMP_PWR_ON_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, Main_Reg_PWR_ON_Pin|LED1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : TMP_INT_Pin PEDO_INT2_Pin PEDO_INT1_Pin */
  GPIO_InitStruct.Pin = TMP_INT_Pin|PEDO_INT2_Pin|PEDO_INT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : RFM_RST_Pin LED3_Pin RFM95W_NSS_Pin MPU_PWR_ON_Pin
                           MCU_BCK_PWR_ON_Pin SOC_CHK_ON_Pin */
  GPIO_InitStruct.Pin = RFM_RST_Pin|LED3_Pin|RFM95W_NSS_Pin|MPU_PWR_ON_Pin
                          |MCU_BCK_PWR_ON_Pin|SOC_CHK_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : RFM_PWR_ON_Pin Main_Reg_PWR_ON_Pin PEDO_PWR_Pin LED1_Pin */
  GPIO_InitStruct.Pin = RFM_PWR_ON_Pin|Main_Reg_PWR_ON_Pin|PEDO_PWR_Pin|LED1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin Pedo_NSS_Pin TMP_PWR_ON_Pin */
  GPIO_InitStruct.Pin = LED2_Pin|Pedo_NSS_Pin|TMP_PWR_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_Pin */
  GPIO_InitStruct.Pin = BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RFM_DIO0_Pin */
  GPIO_InitStruct.Pin = RFM_DIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RFM_DIO0_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RFM_DIO1_Pin RFM_DIO2_Pin MPU_INT_Pin */
  GPIO_InitStruct.Pin = RFM_DIO1_Pin|RFM_DIO2_Pin|MPU_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */