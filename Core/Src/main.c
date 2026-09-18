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
#include "event_episode_service.h"
#include "telemetry_window_service.h"
#include "telemetry_codec.h"
#include "radio_tx_service.h"
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
/*
 * STOP2 is intentionally entered in short watchdog-safe chunks. The IWDG and
 * RTC are both clocked from LSI, so the wakeup margin remains large even when
 * the absolute LSI frequency drifts. Application time is reconstructed from
 * the RTC calendar after every wake so 15-minute/event deadlines continue to
 * advance while SysTick is suspended.
 */
#define LOW_POWER_MAX_IDLE_MS                 20000UL
#define LOW_POWER_DEADLINE_GUARD_MS             250UL
#define LOW_POWER_MIN_STOP_MS                  1250UL

#define LOW_POWER_RTC_BDCR_RTCSEL_MASK       (3UL << 8)
#define LOW_POWER_RTC_BDCR_RTCSEL_LSI        (2UL << 8)
#define LOW_POWER_RTC_BDCR_RTCEN             (1UL << 15)
#define LOW_POWER_RTC_BDCR_BDRST             (1UL << 16)

#define LOW_POWER_RTC_ISR_WUTWF              (1UL << 2)
#define LOW_POWER_RTC_ISR_INITS              (1UL << 4)
#define LOW_POWER_RTC_ISR_INITF              (1UL << 6)
#define LOW_POWER_RTC_ISR_INIT               (1UL << 7)
#define LOW_POWER_RTC_ISR_WUTF               (1UL << 10)

#define LOW_POWER_RTC_CR_WUCKSEL_MASK        (7UL << 0)
#define LOW_POWER_RTC_CR_WUCKSEL_CK_SPRE     (4UL << 0)
#define LOW_POWER_RTC_CR_BYPSHAD             (1UL << 5)
#define LOW_POWER_RTC_CR_WUTE                (1UL << 10)
#define LOW_POWER_RTC_CR_WUTIE               (1UL << 14)

#define LOW_POWER_RTC_EXTI_LINE              (1UL << 20)
#define LOW_POWER_RTC_PREDIV_A               127UL
#define LOW_POWER_RTC_PREDIV_S               249UL
#define LOW_POWER_RTC_PRER_VALUE             ((LOW_POWER_RTC_PREDIV_A << 16) | LOW_POWER_RTC_PREDIV_S)
#define LOW_POWER_RTC_VALID_DATE             0x00002101UL
#define LOW_POWER_RTC_DAY_MS                 86400000UL
#define LOW_POWER_WAIT_LIMIT                 1000000UL
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

/* Managed raw-LoRa TX staging. */
radio_tx_service_status_t radio_tx_service_init_status = RADIO_TX_SERVICE_ERROR_NOT_READY;
radio_tx_service_status_t radio_tx_service_submit_status = RADIO_TX_SERVICE_ERROR_NOT_READY;
bool radio_tx_service_ready = false;
uint32_t telemetry_payload_v2_2_queued_count = 0U;

/* BMA456 raw SPI Phase 4 regression diagnostics. */
uint8_t bma456_first_read = 0U;
uint8_t bma456_chip_id = 0U;
HAL_StatusTypeDef bma456_spi_status = HAL_ERROR;
bool bma456_chip_id_ok = false;
HAL_StatusTypeDef bma456_pwr_status = HAL_ERROR;
uint8_t bma456_pwr_conf_1 = 0U;
uint8_t bma456_pwr_conf_2 = 0U;
uint8_t bma456_chip_id_3 = 0U;

/* Shared Phase 5 runtime configuration used by the staged services. */
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
uint32_t bma_event_service_read_failure_count = 0U;
uint32_t radio_critical_sensor_defer_count = 0U;
uint32_t telemetry_backpressure_defer_count = 0U;
static bool s_telemetry_backpressure_latched = false;

/* TMP117 SensorService diagnostics. */
sensor_service_status_t tmp_service_init_status = SENSOR_SERVICE_ERROR_TMP_INIT;
sensor_service_status_t tmp_service_read_status = SENSOR_SERVICE_ERROR_TMP_READ;
sensor_service_temperature_sample_t tmp_service_sample = {0};
bool tmp_service_ready = false;
uint32_t tmp_service_last_read_tick = 0U;

/* Event Episode diagnostics and non-blocking thermal policy. */
event_episode_service_t event_episode_service = {0};
event_episode_status_t event_episode_service_status = EVENT_EPISODE_ERROR_NOT_INITIALIZED;
event_episode_action_t event_episode_action = {0};
event_episode_summary_t event_episode_last_closed_summary = {0};
bool event_episode_ready = false;
uint32_t event_episode_motion_pulse_count = 0U;
uint32_t event_episode_retrigger_suppressed_count = 0U;
uint32_t event_episode_temperature_sample_count = 0U;
uint32_t event_episode_pulse_temperature_count = 0U;
uint32_t event_episode_followup_temperature_count = 0U;
uint32_t event_episode_temperature_failure_count = 0U;
uint32_t event_episode_closed_count = 0U;
int32_t event_episode_last_temperature_mdeg_c = 0;

/* MPU6050 SensorService + Event Episode diagnostics. */
sensor_service_status_t mpu_service_init_status = SENSOR_SERVICE_ERROR_MPU_INIT;
sensor_service_status_t mpu_service_read_status = SENSOR_SERVICE_ERROR_MPU_READ;
sensor_service_mpu_sample_t mpu_service_sample = {0};
sensor_service_mpu_burst_features_t mpu_service_burst_features = {0};
event_episode_mpu_features_t event_episode_mpu_features = {0};
bool mpu_service_ready = false;
uint32_t mpu_service_last_read_tick = 0U;
uint32_t event_episode_mpu_burst_count = 0U;
uint32_t event_episode_mpu_burst_failure_count = 0U;
uint16_t event_episode_last_mpu_sample_count = 0U;
uint16_t event_episode_last_mpu_peak_gyro_dps = 0U;
uint16_t event_episode_last_mpu_orientation_change_cdeg = 0U;

/* 15-minute telemetry snapshot and encoding diagnostics. */
telemetry_window_service_t telemetry_window_service = {0};
telemetry_window_status_t telemetry_window_status = TELEMETRY_WINDOW_ERROR_CONFIG;
bolus_telemetry_summary_v2_2_t telemetry_frozen_summary_v2_2 = {0};
telemetry_codec_status_t telemetry_codec_status = TELEMETRY_CODEC_ERROR_PARAM;
uint8_t telemetry_payload_v2_2[BOLUS_TELEMETRY_SUMMARY_V2_2_SIZE] = {0};
size_t telemetry_payload_v2_2_size = 0U;
bool telemetry_window_ready = false;
bool telemetry_payload_v2_2_ready = false;
uint32_t telemetry_snapshot_count = 0U;
uint32_t telemetry_snapshot_failure_count = 0U;
uint16_t telemetry_last_battery_mv = 0U;

/* Phase 6 STOP2 diagnostics, intentionally debugger-visible. */
bool low_power_rtc_ready = false;
volatile uint32_t low_power_stop2_entry_count = 0U;
volatile uint32_t low_power_rtc_wake_count = 0U;
volatile uint32_t low_power_early_wake_count = 0U;
volatile uint32_t low_power_last_sleep_ms = 0U;
volatile uint32_t low_power_total_sleep_ms = 0U;
volatile uint32_t low_power_reject_busy_count = 0U;
static volatile bool s_low_power_rtc_irq_seen = false;
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
static bool LowPower_RtcInit(void);
static bool LowPower_TryEnterStop2(uint32_t idle_budget_ms);
static uint32_t LowPower_ComputeIdleBudget(uint32_t now_ms);
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

static void LowPower_RtcUnlock(void)
{
    RTC->WPR = 0xCAU;
    RTC->WPR = 0x53U;
}

static void LowPower_RtcLock(void)
{
    RTC->WPR = 0xFFU;
}

static bool LowPower_WaitRtcFlag(uint32_t mask, bool set)
{
    uint32_t timeout = LOW_POWER_WAIT_LIMIT;

    while (timeout > 0U)
    {
        bool state = ((RTC->ISR & mask) != 0U);
        if (state == set)
        {
            return true;
        }
        timeout--;
    }

    return false;
}

static bool LowPower_RtcInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* SystemClock_Config already enables LSI for IWDG. Select it for RTC too. */
    if ((RCC->BDCR & LOW_POWER_RTC_BDCR_RTCSEL_MASK) !=
        LOW_POWER_RTC_BDCR_RTCSEL_LSI)
    {
        RCC->BDCR |= LOW_POWER_RTC_BDCR_BDRST;
        RCC->BDCR &= ~LOW_POWER_RTC_BDCR_BDRST;
        RCC->BDCR =
            (RCC->BDCR & ~LOW_POWER_RTC_BDCR_RTCSEL_MASK) |
            LOW_POWER_RTC_BDCR_RTCSEL_LSI;
    }

    RCC->BDCR |= LOW_POWER_RTC_BDCR_RTCEN;

    LowPower_RtcUnlock();

    if ((RTC->ISR & LOW_POWER_RTC_ISR_INITS) == 0U)
    {
        RTC->ISR |= LOW_POWER_RTC_ISR_INIT;
        if (!LowPower_WaitRtcFlag(LOW_POWER_RTC_ISR_INITF, true))
        {
            LowPower_RtcLock();
            return false;
        }

        RTC->PRER = LOW_POWER_RTC_PRER_VALUE;
        RTC->TR = 0U;
        RTC->DR = LOW_POWER_RTC_VALID_DATE;
        RTC->ISR &= ~LOW_POWER_RTC_ISR_INIT;
    }

    /* Direct coherent reads avoid a shadow-register synchronization dependency. */
    RTC->CR |= LOW_POWER_RTC_CR_BYPSHAD;
    LowPower_RtcLock();

    EXTI->PR1 = LOW_POWER_RTC_EXTI_LINE;
    EXTI->RTSR1 |= LOW_POWER_RTC_EXTI_LINE;
    EXTI->FTSR1 &= ~LOW_POWER_RTC_EXTI_LINE;
    EXTI->IMR1 |= LOW_POWER_RTC_EXTI_LINE;

    HAL_NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
    HAL_NVIC_SetPriority(RTC_WKUP_IRQn, 7U, 0U);
    HAL_NVIC_EnableIRQ(RTC_WKUP_IRQn);

    return true;
}

static uint32_t LowPower_RtcTimeOfDayMs(void)
{
    uint32_t tr_first;
    uint32_t tr_second;
    uint32_t ssr;
    uint32_t hours;
    uint32_t minutes;
    uint32_t seconds;
    uint32_t subsecond_ms;

    do
    {
        tr_first = RTC->TR;
        ssr = RTC->SSR;
        tr_second = RTC->TR;
    } while (tr_first != tr_second);

    hours = (((tr_first >> 20) & 0x3U) * 10U) + ((tr_first >> 16) & 0xFU);
    minutes = (((tr_first >> 12) & 0x7U) * 10U) + ((tr_first >> 8) & 0xFU);
    seconds = (((tr_first >> 4) & 0x7U) * 10U) + (tr_first & 0xFU);

    ssr &= 0x7FFFU;
    if (ssr > LOW_POWER_RTC_PREDIV_S)
    {
        ssr = LOW_POWER_RTC_PREDIV_S;
    }
    subsecond_ms =
        ((LOW_POWER_RTC_PREDIV_S - ssr) * 1000UL) /
        (LOW_POWER_RTC_PREDIV_S + 1UL);

    return (((hours * 60UL + minutes) * 60UL + seconds) * 1000UL) +
           subsecond_ms;
}

static bool LowPower_ArmWakeup(uint32_t seconds)
{
    if ((seconds == 0U) || (seconds > 0xFFFFU))
    {
        return false;
    }

    LowPower_RtcUnlock();
    RTC->CR &= ~(LOW_POWER_RTC_CR_WUTE | LOW_POWER_RTC_CR_WUTIE);

    if (!LowPower_WaitRtcFlag(LOW_POWER_RTC_ISR_WUTWF, true))
    {
        LowPower_RtcLock();
        return false;
    }

    RTC->ISR &= ~LOW_POWER_RTC_ISR_WUTF;
    RTC->WUTR = seconds - 1U;
    RTC->CR =
        (RTC->CR & ~LOW_POWER_RTC_CR_WUCKSEL_MASK) |
        LOW_POWER_RTC_CR_WUCKSEL_CK_SPRE;
    RTC->CR |= (LOW_POWER_RTC_CR_WUTIE | LOW_POWER_RTC_CR_WUTE);
    LowPower_RtcLock();

    s_low_power_rtc_irq_seen = false;
    EXTI->PR1 = LOW_POWER_RTC_EXTI_LINE;
    HAL_NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
    return true;
}

static void LowPower_DisarmWakeup(void)
{
    LowPower_RtcUnlock();
    RTC->CR &= ~(LOW_POWER_RTC_CR_WUTE | LOW_POWER_RTC_CR_WUTIE);
    (void)LowPower_WaitRtcFlag(LOW_POWER_RTC_ISR_WUTWF, true);
    RTC->ISR &= ~LOW_POWER_RTC_ISR_WUTF;
    LowPower_RtcLock();
    EXTI->PR1 = LOW_POWER_RTC_EXTI_LINE;
    HAL_NVIC_ClearPendingIRQ(RTC_WKUP_IRQn);
}

static uint32_t LowPower_TimeUntil(uint32_t now_ms, uint32_t deadline_ms)
{
    int32_t delta = (int32_t)(deadline_ms - now_ms);
    return (delta > 0) ? (uint32_t)delta : 0U;
}

static void LowPower_TightenBudget(
    uint32_t now_ms,
    uint32_t deadline_ms,
    uint32_t *budget_ms)
{
    uint32_t until_deadline;

    if (budget_ms == NULL)
    {
        return;
    }

    until_deadline = LowPower_TimeUntil(now_ms, deadline_ms);
    if (until_deadline < *budget_ms)
    {
        *budget_ms = until_deadline;
    }
}

static uint32_t LowPower_ComputeIdleBudget(uint32_t now_ms)
{
    uint32_t budget_ms = LOW_POWER_MAX_IDLE_MS;

    if (telemetry_window_ready)
    {
        LowPower_TightenBudget(
            now_ms,
            telemetry_window_service.window_start_ms +
                telemetry_window_service.uplink_period_ms,
            &budget_ms);
    }

    if (tmp_service_ready)
    {
        LowPower_TightenBudget(
            now_ms,
            tmp_service_last_read_tick +
                (sensor_service_config.temperature.sample_period_s * 1000UL),
            &budget_ms);
    }

    if (event_episode_ready && event_episode_service.active)
    {
        LowPower_TightenBudget(
            now_ms,
            event_episode_service.close_deadline_ms,
            &budget_ms);

        if (event_episode_service.followup_active)
        {
            LowPower_TightenBudget(
                now_ms,
                event_episode_service.next_followup_due_ms,
                &budget_ms);
        }
    }

    return budget_ms;
}

static bool LowPower_CanEnterStop2(void)
{
    if (!low_power_rtc_ready)
    {
        return false;
    }

    if (telemetry_payload_v2_2_ready)
    {
        return false;
    }

    if (radio_tx_service_ready && RadioTxService_IsBusy())
    {
        low_power_reject_busy_count++;
        return false;
    }

    if (bma_event_service_ready &&
        (bma_irq_diag_count != bma_event_service_processed_irq_count))
    {
        return false;
    }

    return true;
}

static bool LowPower_TryEnterStop2(uint32_t idle_budget_ms)
{
    uint32_t sleep_seconds;
    uint32_t rtc_before_ms;
    uint32_t rtc_after_ms;
    uint32_t elapsed_ms;
    uint32_t tick_before;
    uint32_t tick_advanced;

    if (!LowPower_CanEnterStop2() ||
        (idle_budget_ms < LOW_POWER_MIN_STOP_MS))
    {
        return false;
    }

    if (idle_budget_ms <= LOW_POWER_DEADLINE_GUARD_MS)
    {
        return false;
    }

    sleep_seconds =
        (idle_budget_ms - LOW_POWER_DEADLINE_GUARD_MS) / 1000UL;
    if (sleep_seconds == 0U)
    {
        return false;
    }

    rtc_before_ms = LowPower_RtcTimeOfDayMs();
    if (!LowPower_ArmWakeup(sleep_seconds))
    {
        return false;
    }

    /*
     * Re-check asynchronous work after arming the RTC. This closes the large
     * part of the check-to-WFI race: a BMA edge or radio DIO that arrived while
     * preparing STOP2 is serviced on the next scheduler pass instead of being
     * delayed by a full sleep chunk.
     */
    if ((bma_event_service_ready &&
         (bma_irq_diag_count != bma_event_service_processed_irq_count)) ||
        (radio_tx_service_ready && RadioTxService_IsRadioCritical()))
    {
        LowPower_DisarmWakeup();
        return false;
    }

    HAL_IWDG_Refresh(&hiwdg);
    tick_before = uwTick;
    HAL_SuspendTick();

    low_power_stop2_entry_count++;
    __DSB();
    __ISB();
    HAL_PWREx_EnterSTOP2Mode(PWR_STOPENTRY_WFI);

    /* STOP2 disables the PLL/HSE system clock; restore it before normal work. */
    SystemClock_Config();
    rtc_after_ms = LowPower_RtcTimeOfDayMs();
    LowPower_DisarmWakeup();

    if (rtc_after_ms >= rtc_before_ms)
    {
        elapsed_ms = rtc_after_ms - rtc_before_ms;
    }
    else
    {
        elapsed_ms = (LOW_POWER_RTC_DAY_MS - rtc_before_ms) + rtc_after_ms;
    }

    /* HAL_RCC_ClockConfig may have restarted SysTick while restoring clocks. */
    tick_advanced = uwTick - tick_before;
    if (elapsed_ms > tick_advanced)
    {
        uwTick += (elapsed_ms - tick_advanced);
    }

    HAL_ResumeTick();
    HAL_IWDG_Refresh(&hiwdg);

    low_power_last_sleep_ms = elapsed_ms;
    low_power_total_sleep_ms += elapsed_ms;
    if (!s_low_power_rtc_irq_seen)
    {
        low_power_early_wake_count++;
    }

    return true;
}

/* RTC wakeup is intentionally lower priority than BMA and RFM DIO IRQs. */
void RTC_WKUP_IRQHandler(void)
{
    if ((RTC->ISR & LOW_POWER_RTC_ISR_WUTF) != 0U)
    {
        LowPower_RtcUnlock();
        RTC->ISR &= ~LOW_POWER_RTC_ISR_WUTF;
        LowPower_RtcLock();
        s_low_power_rtc_irq_seen = true;
        low_power_rtc_wake_count++;
    }

    EXTI->PR1 = LOW_POWER_RTC_EXTI_LINE;
}

static void HandleEventEpisodeAction(const event_episode_action_t *action)
{
    if (action == NULL)
    {
        return;
    }

    if (telemetry_window_ready)
    {
        TelemetryWindow_RecordEpisodeAction(&telemetry_window_service, action);
    }

    if (action->pulse_accepted)
    {
        event_episode_motion_pulse_count++;
    }

    if (action->pulse_suppressed_by_guard)
    {
        event_episode_retrigger_suppressed_count++;
    }

    if (action->episode_closed)
    {
        if (EventEpisodeService_GetLastClosedSummary(
                &event_episode_service,
                &event_episode_last_closed_summary) == EVENT_EPISODE_OK)
        {
            event_episode_closed_count++;
        }
    }

    if (action->take_temperature_now)
    {
        if (!tmp_service_ready)
        {
            event_episode_temperature_failure_count++;
        }
        else
        {
            tmp_service_read_status =
                SensorService_ReadTemperatureOneShot(&tmp_service_sample);
            tmp_service_last_read_tick = HAL_GetTick();

            if (tmp_service_read_status != SENSOR_SERVICE_OK)
            {
                event_episode_temperature_failure_count++;
            }
            else
            {
                event_episode_last_temperature_mdeg_c =
                    tmp_service_sample.temperature_mdeg_c;
                event_episode_temperature_sample_count++;

                if ((action->temperature_source ==
                     EVENT_EPISODE_TEMP_SOURCE_FIRST_PULSE) ||
                    (action->temperature_source ==
                     EVENT_EPISODE_TEMP_SOURCE_MOTION_PULSE))
                {
                    event_episode_pulse_temperature_count++;
                }
                else if (action->temperature_source ==
                         EVENT_EPISODE_TEMP_SOURCE_FOLLOWUP)
                {
                    event_episode_followup_temperature_count++;
                }

                event_episode_service_status =
                    EventEpisodeService_RecordTemperature(
                        &event_episode_service,
                        action->temperature_source,
                        tmp_service_sample.temperature_mdeg_c);

                if (telemetry_window_ready)
                {
                    TelemetryWindow_RecordTemperature(
                        &telemetry_window_service,
                        tmp_service_sample.temperature_mdeg_c);
                }
            }
        }
    }

    if (action->take_mpu_burst_now &&
        sensor_service_config.mpu.event_trigger_enable)
    {
        if (!mpu_service_ready)
        {
            event_episode_mpu_burst_failure_count++;
        }
        else
        {
            HAL_IWDG_Refresh(&hiwdg);

            mpu_service_read_status =
                SensorService_ReadMpuBurst(&mpu_service_burst_features);
            mpu_service_last_read_tick = HAL_GetTick();

            HAL_IWDG_Refresh(&hiwdg);

            if (mpu_service_read_status != SENSOR_SERVICE_OK)
            {
                event_episode_mpu_burst_failure_count++;
            }
            else
            {
                event_episode_mpu_features.sample_count =
                    mpu_service_burst_features.sample_count;
                event_episode_mpu_features.peak_dynamic_accel_mg =
                    mpu_service_burst_features.peak_dynamic_accel_mg;
                event_episode_mpu_features.rms_dynamic_accel_mg =
                    mpu_service_burst_features.rms_dynamic_accel_mg;
                event_episode_mpu_features.peak_angular_velocity_dps =
                    mpu_service_burst_features.peak_angular_velocity_dps;
                event_episode_mpu_features.rms_angular_velocity_dps =
                    mpu_service_burst_features.rms_angular_velocity_dps;
                event_episode_mpu_features.total_angular_motion_cdeg =
                    mpu_service_burst_features.total_angular_motion_cdeg;
                event_episode_mpu_features.orientation_change_valid =
                    mpu_service_burst_features.orientation_change_valid;
                event_episode_mpu_features.orientation_change_cdeg =
                    mpu_service_burst_features.orientation_change_cdeg;

                event_episode_service_status =
                    EventEpisodeService_RecordMpuBurst(
                        &event_episode_service,
                        &event_episode_mpu_features);

                if (event_episode_service_status == EVENT_EPISODE_OK)
                {
                    event_episode_mpu_burst_count++;
                    event_episode_last_mpu_sample_count =
                        mpu_service_burst_features.sample_count;
                    event_episode_last_mpu_peak_gyro_dps =
                        mpu_service_burst_features.peak_angular_velocity_dps;
                    event_episode_last_mpu_orientation_change_cdeg =
                        mpu_service_burst_features.orientation_change_cdeg;

                    if (telemetry_window_ready)
                    {
                        TelemetryWindow_RecordMpuBurst(
                            &telemetry_window_service,
                            &mpu_service_burst_features);
                    }
                }
                else
                {
                    event_episode_mpu_burst_failure_count++;
                }
            }
        }
    }
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

  event_episode_service_status =
      EventEpisodeService_Init(&event_episode_service, &sensor_service_config);
  event_episode_ready = (event_episode_service_status == EVENT_EPISODE_OK);

  telemetry_window_status =
      TelemetryWindow_Init(
          &telemetry_window_service,
          &sensor_service_config,
          HAL_GetTick());
  telemetry_window_ready = (telemetry_window_status == TELEMETRY_WINDOW_OK);

  bma456_service_init_status = SensorService_InitBma(&hspi2, &sensor_service_config);
  bma456_service_ready =
      ((bma456_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsBmaReady());
  if (bma456_service_ready)
  {
      /*
       * Do not poll Step/XYZ here or every 500 ms. BMA456 remains powered and
       * counts steps/raises Any-Motion in hardware; its telemetry snapshot is
       * read once, immediately before the 15-minute telemetry freeze.
       */
      bma456_service_last_read_tick = HAL_GetTick();

      bma_event_service_init_status =
          BmaEventService_Init(&hspi2, &sensor_service_config);
      bma_event_service_ready =
          ((bma_event_service_init_status == BMA_EVENT_SERVICE_OK) &&
           BmaEventService_IsReady());
  }

  tmp_service_init_status = SensorService_InitTemperature(&hi2c3, &sensor_service_config);
  tmp_service_ready =
      ((tmp_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsTemperatureReady());
  if (tmp_service_ready)
  {
      tmp_service_read_status = SensorService_ReadTemperatureOneShot(&tmp_service_sample);
      tmp_service_last_read_tick = HAL_GetTick();

      if ((tmp_service_read_status == SENSOR_SERVICE_OK) && telemetry_window_ready)
      {
          TelemetryWindow_RecordTemperature(
              &telemetry_window_service,
              tmp_service_sample.temperature_mdeg_c);
      }
  }

  /* MPU6050 is configured once, then remains physically off until event burst. */
  mpu_service_init_status = SensorService_InitMpu(&hi2c1, &sensor_service_config);
  mpu_service_ready =
      ((mpu_service_init_status == SENSOR_SERVICE_OK) && SensorService_IsMpuReady());

  /*
   * RFM95W Phase-4 regression remains intact. Attach managed TX callbacks before
   * SX1276Init so the driver retains TxDone/TxTimeout function pointers.
   */
  BolusPower_On(BOLUS_POWER_RFM95W);
  HAL_Delay(RFM95W_POWERUP_DELAY_MS);
  Sx_Board_Bus_Init();
  Sx_Board_IoInit();
  RadioTxService_AttachEvents(&rfm95w_radio_events);
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

  radio_tx_service_init_status = RadioTxService_Init(&sensor_service_config);
  radio_tx_service_ready =
      ((radio_tx_service_init_status == RADIO_TX_SERVICE_OK) &&
       RadioTxService_IsReady());

  low_power_rtc_ready = LowPower_RtcInit();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now_ms = HAL_GetTick();

    TimerProcess();

    /* Process deferred radio DIO and timeout state in cooperative main context. */
    if (radio_tx_service_ready)
    {
        RadioTxService_Process(now_ms);
    }

    /*
     * TMP117 one-shot reads and MPU6050 bursts are blocking. Never start them
     * while LoRaMAC is inside TX/RX1/RX2 or has an unprocessed DIO/MAC event.
     * This preserves the TxDone -> RX-window timing contract.
     */
    if (bma_event_service_ready &&
        (!radio_tx_service_ready || !RadioTxService_IsRadioCritical()))
    {
        uint32_t irq_count_snapshot = bma_irq_diag_count;

        if (irq_count_snapshot != bma_event_service_processed_irq_count)
        {
            bma_event_service_read_status =
                BmaEventService_Read(&bma_event_service_sample);

            /*
             * Consume the IRQ even on a sensor read failure. The service has
             * already raised a communication fault; keeping the count pending
             * forever would otherwise prevent STOP2 and drain the battery.
             */
            bma_event_service_processed_irq_count = irq_count_snapshot;

            if (bma_event_service_read_status == BMA_EVENT_SERVICE_OK)
            {
                bma_event_service_ack_count++;

                if (bma_event_service_sample.any_motion)
                {
                    bma_event_service_any_motion_count++;

                    if (event_episode_ready)
                    {
                        event_episode_service_status =
                            EventEpisodeService_OnMotionPulse(
                                &event_episode_service,
                                now_ms,
                                &event_episode_action);

                        if (event_episode_service_status == EVENT_EPISODE_OK)
                        {
                            HandleEventEpisodeAction(&event_episode_action);
                        }
                    }
                }
            }
            else
            {
                bma_event_service_read_failure_count++;
            }
        }
    }
    else if (bma_event_service_ready &&
             (bma_irq_diag_count != bma_event_service_processed_irq_count))
    {
        radio_critical_sensor_defer_count++;
    }

    if (event_episode_ready &&
        (!radio_tx_service_ready || !RadioTxService_IsRadioCritical()))
    {
        event_episode_service_status =
            EventEpisodeService_Poll(
                &event_episode_service,
                now_ms,
                &event_episode_action);

        if (event_episode_service_status == EVENT_EPISODE_OK)
        {
            HandleEventEpisodeAction(&event_episode_action);
        }
    }

    if (tmp_service_ready &&
        (!radio_tx_service_ready || !RadioTxService_IsRadioCritical()) &&
        ((HAL_GetTick() - tmp_service_last_read_tick) >=
         (sensor_service_config.temperature.sample_period_s * 1000UL)))
    {
        tmp_service_last_read_tick = HAL_GetTick();
        tmp_service_read_status = SensorService_ReadTemperatureOneShot(&tmp_service_sample);

        if ((tmp_service_read_status == SENSOR_SERVICE_OK) && telemetry_window_ready)
        {
            TelemetryWindow_RecordTemperature(
                &telemetry_window_service,
                tmp_service_sample.temperature_mdeg_c);
        }
    }

    /*
     * At every 15-minute boundary take the final measurements, freeze the
     * window and encode compact Telemetry V2.2. BMA456 Step + XYZ is sampled
     * exactly once here; there is no 500 ms BMA polling path anymore.
     */
    if (telemetry_window_ready &&
        !telemetry_payload_v2_2_ready &&
        (!radio_tx_service_ready || !RadioTxService_IsRadioCritical()) &&
        TelemetryWindow_IsDue(&telemetry_window_service, HAL_GetTick()))
    {
        battery_status_t battery_mv_status;
        bolus_health_status_t health;
        bool fault_present;

        if (tmp_service_ready)
        {
            tmp_service_read_status =
                SensorService_ReadTemperatureOneShot(&tmp_service_sample);
            tmp_service_last_read_tick = HAL_GetTick();

            if (tmp_service_read_status == SENSOR_SERVICE_OK)
            {
                TelemetryWindow_RecordTemperature(
                    &telemetry_window_service,
                    tmp_service_sample.temperature_mdeg_c);
            }
        }

        if (bma456_service_ready)
        {
            bma456_service_read_status =
                SensorService_ReadBmaSample(&bma456_service_sample);
            bma456_service_last_read_tick = HAL_GetTick();

            if (bma456_service_read_status == SENSOR_SERVICE_OK)
            {
                TelemetryWindow_RecordBma456(
                    &telemetry_window_service,
                    bma456_service_sample.step_total,
                    bma456_service_sample.x_mg,
                    bma456_service_sample.y_mg,
                    bma456_service_sample.z_mg);
            }
        }

        battery_mv_status = Battery_ReadMillivolts(&telemetry_last_battery_mv);

        if (battery_mv_status != BATTERY_OK)
        {
            FaultManager_Raise(BOLUS_FAULT_BATTERY_MEASUREMENT);
        }
        else
        {
            (void)FaultManager_ClearFault(BOLUS_FAULT_BATTERY_MEASUREMENT);
        }

        health = FaultManager_GetHealth();
        fault_present = (FaultManager_GetActiveMask() != 0U);

        telemetry_window_status =
            TelemetryWindow_FreezeSummaryV2_2(
                &telemetry_window_service,
                &sensor_service_config,
                HAL_GetTick(),
                telemetry_last_battery_mv,
                fault_present,
                (health == BOLUS_HEALTH_DEGRADED),
                (health == BOLUS_HEALTH_CRITICAL),
                &telemetry_frozen_summary_v2_2);

        if (telemetry_window_status == TELEMETRY_WINDOW_OK)
        {
            telemetry_codec_status =
                TelemetryCodec_EncodeSummaryV2_2(
                    &telemetry_frozen_summary_v2_2,
                    telemetry_payload_v2_2,
                    sizeof(telemetry_payload_v2_2),
                    &telemetry_payload_v2_2_size);

            if (telemetry_codec_status == TELEMETRY_CODEC_OK)
            {
                telemetry_payload_v2_2_ready = true;
                telemetry_snapshot_count++;
            }
            else
            {
                telemetry_snapshot_failure_count++;
            }
        }
        else
        {
            telemetry_snapshot_failure_count++;
        }
    }

    if (telemetry_window_ready &&
        telemetry_payload_v2_2_ready &&
        TelemetryWindow_IsDue(&telemetry_window_service, HAL_GetTick()))
    {
        if (!s_telemetry_backpressure_latched)
        {
            telemetry_backpressure_defer_count++;
            s_telemetry_backpressure_latched = true;
        }
    }
    else if (!telemetry_payload_v2_2_ready)
    {
        s_telemetry_backpressure_latched = false;
    }

    /*
     * Transfer ownership of a frozen/encoded packet to the TX service exactly
     * once. If the TX service is busy, keep telemetry_payload_v2_2_ready=true and
     * try again on a later loop; the source buffer is not modified meanwhile.
     */
    if (radio_tx_service_ready &&
        telemetry_payload_v2_2_ready &&
        (telemetry_payload_v2_2_size > 0U) &&
        RadioTxService_CanAccept())
    {
        radio_tx_service_submit_status =
            RadioTxService_Submit(
                telemetry_payload_v2_2,
                (uint8_t)telemetry_payload_v2_2_size,
                telemetry_frozen_summary_v2_2.v2.sequence);

        if (radio_tx_service_submit_status == RADIO_TX_SERVICE_OK)
        {
            telemetry_payload_v2_2_ready = false;
            telemetry_payload_v2_2_queued_count++;
        }
    }

    /* Start a newly queued attempt without waiting for another scheduler pass. */
    if (radio_tx_service_ready)
    {
        RadioTxService_Process(HAL_GetTick());
    }

    HAL_IWDG_Refresh(&hiwdg);

    /*
     * STOP2 is allowed only after all cooperative work is drained. The RTC
     * budget is tightened to the next telemetry, temperature or event deadline.
     * BMA INT1 can wake the MCU asynchronously before the RTC wakeup fires.
     */
    now_ms = HAL_GetTick();
    if (!LowPower_TryEnterStop2(LowPower_ComputeIdleBudget(now_ms)))
    {
        HAL_Delay(10U);
    }
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
  /* ~32 s nominal timeout from LSI; STOP2 chunks stay below 20 s. */
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
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
  hspi1.Init.DataSize = SPI_DATASIZE_8B;
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
  hspi2.Init.DataSize = SPI_DATASIZE_8B;
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

  /* BMA INT1 remains isolated on EXTI9_5 by BmaIrqDiag. */
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

  /*
   * Managed TX requires only SX1276 DIO0 (TxDone). DIO1/DIO2 stay ordinary
   * inputs until the receive/downlink stage so they cannot disturb BMA EXTI.
   */
  GPIO_InitStruct.Pin = RFM_DIO0_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RFM_DIO0_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = RFM_DIO1_Pin|RFM_DIO2_Pin|MPU_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 6U, 0U);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
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
