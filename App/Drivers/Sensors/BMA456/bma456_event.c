#include "bma456_event.h"

#include "bma456h.h"
#include "main.h"

#define BMA456_EVENT_SPI_TIMEOUT_MS          20U
#define BMA456_EVENT_MAX_TRANSFER            64U
#define BMA456_EVENT_SAMPLE_PERIOD_MS        20U
#define BMA456_EVENT_Q11_COUNTS_PER_G        2048UL
#define BMA456_EVENT_MAX_THRESHOLD_RAW       0x07FFU

/*
 * Phase-5 bench diagnostics. These remain non-static so CubeIDE Live
 * Expressions can verify what the Bosch feature engine actually contains.
 */
bool bma456_event_diag_readback_valid = false;
bool bma456_event_diag_int1_configured = false;
uint16_t bma456_event_diag_threshold_raw = 0U;
uint16_t bma456_event_diag_threshold_mg = 0U;
uint16_t bma456_event_diag_duration_samples = 0U;
uint32_t bma456_event_diag_duration_ms = 0U;
uint16_t bma456_event_diag_last_raw_status = 0U;

static struct bma4_dev s_event_dev = {0};
static bool s_event_ready = false;
static bool s_event_enabled = false;

extern IWDG_HandleTypeDef hiwdg;

static void EventServiceWatchdog(void)
{
    if (hiwdg.Instance == IWDG)
    {
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}

static BMA4_INTF_RET_TYPE EventSpiRead(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr)
{
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef *)intf_ptr;
    uint8_t dummy_tx[BMA456_EVENT_MAX_TRANSFER] = {0};
    HAL_StatusTypeDef status;

    if ((hspi == NULL) || (reg_data == NULL) ||
        (len == 0U) || (len > BMA456_EVENT_MAX_TRANSFER))
    {
        return (BMA4_INTF_RET_TYPE)1;
    }

    EventServiceWatchdog();

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(
        hspi,
        &reg_addr,
        1U,
        BMA456_EVENT_SPI_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        status = HAL_SPI_TransmitReceive(
            hspi,
            dummy_tx,
            reg_data,
            (uint16_t)len,
            BMA456_EVENT_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ?
        BMA4_INTF_RET_SUCCESS : (BMA4_INTF_RET_TYPE)1;
}

static BMA4_INTF_RET_TYPE EventSpiWrite(
    uint8_t reg_addr,
    const uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr)
{
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef *)intf_ptr;
    HAL_StatusTypeDef status;

    if ((hspi == NULL) || (reg_data == NULL) || (len == 0U))
    {
        return (BMA4_INTF_RET_TYPE)1;
    }

    EventServiceWatchdog();

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(
        hspi,
        &reg_addr,
        1U,
        BMA456_EVENT_SPI_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        status = HAL_SPI_Transmit(
            hspi,
            (uint8_t *)reg_data,
            (uint16_t)len,
            BMA456_EVENT_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ?
        BMA4_INTF_RET_SUCCESS : (BMA4_INTF_RET_TYPE)1;
}

static void EventDelayUs(uint32_t period_us, void *intf_ptr)
{
    uint32_t delay_ms;

    (void)intf_ptr;

    EventServiceWatchdog();

    if (period_us == 0U)
    {
        return;
    }

    delay_ms = period_us / 1000U;
    if ((period_us % 1000U) != 0U)
    {
        delay_ms++;
    }

    HAL_Delay(delay_ms);
}

static bool ThresholdMgToRaw(uint16_t threshold_mg, uint16_t *raw)
{
    uint32_t scaled;

    if ((raw == NULL) || (threshold_mg == 0U) || (threshold_mg > 1000U))
    {
        return false;
    }

    scaled = (((uint32_t)threshold_mg * BMA456_EVENT_Q11_COUNTS_PER_G) + 500UL) /
             1000UL;

    if (scaled > BMA456_EVENT_MAX_THRESHOLD_RAW)
    {
        return false;
    }

    *raw = (uint16_t)scaled;
    return true;
}

static uint16_t ThresholdRawToMg(uint16_t raw)
{
    uint32_t scaled = (((uint32_t)raw * 1000UL) +
                       (BMA456_EVENT_Q11_COUNTS_PER_G / 2UL)) /
                      BMA456_EVENT_Q11_COUNTS_PER_G;

    return (uint16_t)scaled;
}

bma456_event_status_t BMA456Event_Init(
    SPI_HandleTypeDef *hspi,
    const bma456_event_config_t *config)
{
    struct bma456h_any_no_mot_config any_motion = {0};
    struct bma4_int_pin_config int_pin_config = {0};
    uint16_t threshold_raw = 0U;
    int8_t result;

    if ((hspi == NULL) || (config == NULL))
    {
        return BMA456_EVENT_ERROR_PARAM;
    }

    if ((config->threshold_mg > 1000U) ||
        ((config->duration_ms != 0U) &&
         (((config->duration_ms % BMA456_EVENT_SAMPLE_PERIOD_MS) != 0U) ||
          (config->duration_ms > 60000U))))
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    s_event_ready = false;
    s_event_enabled = false;

    bma456_event_diag_readback_valid = false;
    bma456_event_diag_int1_configured = false;
    bma456_event_diag_threshold_raw = 0U;
    bma456_event_diag_threshold_mg = 0U;
    bma456_event_diag_duration_samples = 0U;
    bma456_event_diag_duration_ms = 0U;
    bma456_event_diag_last_raw_status = 0U;

    s_event_dev = (struct bma4_dev){0};
    s_event_dev.intf = BMA4_SPI_INTF;
    s_event_dev.bus_read = EventSpiRead;
    s_event_dev.bus_write = EventSpiWrite;
    s_event_dev.delay_us = EventDelayUs;
    s_event_dev.intf_ptr = hspi;
    s_event_dev.variant = BMA45X_VARIANT;
    s_event_dev.read_write_len = 46U;
    s_event_dev.perf_mode_status = BMA4_DISABLE;

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    /*
     * MotionService has already uploaded the BMA456H feature image. Calling
     * init here only prepares this wrapper's Bosch device context and verifies
     * communication; it does not rewrite the feature image.
     */
    result = bma456h_init(&s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_COMM;
    }

    result = bma456h_get_any_mot_config(&any_motion, &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    if (config->threshold_mg != 0U)
    {
        if (!ThresholdMgToRaw(config->threshold_mg, &threshold_raw))
        {
            return BMA456_EVENT_ERROR_CONFIG;
        }

        any_motion.threshold = threshold_raw;
    }

    if (config->duration_ms != 0U)
    {
        any_motion.duration =
            (uint16_t)(config->duration_ms / BMA456_EVENT_SAMPLE_PERIOD_MS);
    }

    if ((config->threshold_mg != 0U) || (config->duration_ms != 0U))
    {
        result = bma456h_set_any_mot_config(&any_motion, &s_event_dev);
        if (result != BMA4_OK)
        {
            return BMA456_EVENT_ERROR_CONFIG;
        }
    }

    if (!config->enable)
    {
        (void)bma456h_feature_enable(
            BMA456H_ANY_MOTION_ALL_AXIS_EN,
            BMA4_DISABLE,
            &s_event_dev);
        (void)bma456h_map_interrupt(
            BMA4_INTR1_MAP,
            BMA456H_ANY_MOT_INT,
            BMA4_DISABLE,
            &s_event_dev);

        s_event_ready = true;
        return BMA456_EVENT_OK;
    }

    /* Active-high push-pull rising edge matches PC7/PEDO_INT1 EXTI input. */
    int_pin_config.edge_ctrl = BMA4_EDGE_TRIGGER;
    int_pin_config.lvl = BMA4_ACTIVE_HIGH;
    int_pin_config.od = BMA4_PUSH_PULL;
    int_pin_config.output_en = BMA4_OUTPUT_ENABLE;
    int_pin_config.input_en = BMA4_INPUT_DISABLE;

    result = bma4_set_int_pin_config(
        &int_pin_config,
        BMA4_INTR1_MAP,
        &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    result = bma456h_feature_enable(
        BMA456H_ANY_MOTION_ALL_AXIS_EN,
        BMA4_ENABLE,
        &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    result = bma456h_map_interrupt(
        BMA4_INTR1_MAP,
        BMA456H_ANY_MOT_INT,
        BMA4_ENABLE,
        &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    /* Read back the actual feature-image settings for bench evidence. */
    result = bma456h_get_any_mot_config(&any_motion, &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_CONFIG;
    }

    bma456_event_diag_threshold_raw = any_motion.threshold;
    bma456_event_diag_threshold_mg = ThresholdRawToMg(any_motion.threshold);
    bma456_event_diag_duration_samples = any_motion.duration;
    bma456_event_diag_duration_ms =
        (uint32_t)any_motion.duration * BMA456_EVENT_SAMPLE_PERIOD_MS;
    bma456_event_diag_int1_configured = true;
    bma456_event_diag_readback_valid = true;

    s_event_enabled = true;
    s_event_ready = true;

    return BMA456_EVENT_OK;
}

bma456_event_status_t BMA456Event_ReadStatus(
    bool *any_motion,
    uint16_t *raw_interrupt_status)
{
    uint16_t interrupt_status = 0U;
    int8_t result;

    if ((any_motion == NULL) || (raw_interrupt_status == NULL))
    {
        return BMA456_EVENT_ERROR_PARAM;
    }

    *any_motion = false;
    *raw_interrupt_status = 0U;

    if ((!s_event_ready) || (!s_event_enabled))
    {
        return BMA456_EVENT_ERROR_NOT_READY;
    }

    result = bma456h_read_int_status(&interrupt_status, &s_event_dev);
    if (result != BMA4_OK)
    {
        return BMA456_EVENT_ERROR_COMM;
    }

    bma456_event_diag_last_raw_status = interrupt_status;
    *raw_interrupt_status = interrupt_status;
    *any_motion = ((interrupt_status & BMA456H_ANY_MOT_INT) != 0U);

    return BMA456_EVENT_OK;
}

bool BMA456Event_IsReady(void)
{
    return s_event_ready && s_event_enabled;
}
