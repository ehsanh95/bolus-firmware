#include "bma456_motion.h"

#include "bma456h.h"
#include "main.h"

#define BMA456_MOTION_SPI_TIMEOUT_MS  20U
#define BMA456_MOTION_MAX_TRANSFER    64U

static struct bma4_dev s_dev = {0};
static bool s_ready = false;
static uint8_t s_range_g = 4U;

static BMA4_INTF_RET_TYPE MotionSpiRead(
    uint8_t reg_addr,
    uint8_t *reg_data,
    uint32_t len,
    void *intf_ptr)
{
    SPI_HandleTypeDef *hspi = (SPI_HandleTypeDef *)intf_ptr;
    uint8_t dummy_tx[BMA456_MOTION_MAX_TRANSFER] = {0};
    HAL_StatusTypeDef status;

    if ((hspi == NULL) || (reg_data == NULL) ||
        (len == 0U) || (len > BMA456_MOTION_MAX_TRANSFER))
    {
        return (BMA4_INTF_RET_TYPE)1;
    }

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(
        hspi,
        &reg_addr,
        1U,
        BMA456_MOTION_SPI_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        status = HAL_SPI_TransmitReceive(
            hspi,
            dummy_tx,
            reg_data,
            (uint16_t)len,
            BMA456_MOTION_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ?
        BMA4_INTF_RET_SUCCESS : (BMA4_INTF_RET_TYPE)1;
}

static BMA4_INTF_RET_TYPE MotionSpiWrite(
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

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_RESET);

    status = HAL_SPI_Transmit(
        hspi,
        &reg_addr,
        1U,
        BMA456_MOTION_SPI_TIMEOUT_MS);

    if (status == HAL_OK)
    {
        status = HAL_SPI_Transmit(
            hspi,
            (uint8_t *)reg_data,
            (uint16_t)len,
            BMA456_MOTION_SPI_TIMEOUT_MS);
    }

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    return (status == HAL_OK) ?
        BMA4_INTF_RET_SUCCESS : (BMA4_INTF_RET_TYPE)1;
}

static void MotionDelayUs(uint32_t period_us, void *intf_ptr)
{
    uint32_t delay_ms;

    (void)intf_ptr;

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

static bool MapOdr(bma456_motion_odr_t odr, uint8_t *bosch_odr)
{
    if (bosch_odr == NULL)
    {
        return false;
    }

    switch (odr)
    {
        case BMA456_MOTION_ODR_6_25_HZ:
            *bosch_odr = BMA4_OUTPUT_DATA_RATE_6_25HZ;
            return true;

        case BMA456_MOTION_ODR_12_5_HZ:
            *bosch_odr = BMA4_OUTPUT_DATA_RATE_12_5HZ;
            return true;

        case BMA456_MOTION_ODR_25_HZ:
            *bosch_odr = BMA4_OUTPUT_DATA_RATE_25HZ;
            return true;

        case BMA456_MOTION_ODR_50_HZ:
            *bosch_odr = BMA4_OUTPUT_DATA_RATE_50HZ;
            return true;

        default:
            return false;
    }
}

static bool MapRange(uint8_t range_g, uint8_t *bosch_range)
{
    if (bosch_range == NULL)
    {
        return false;
    }

    switch (range_g)
    {
        case 2U:
            *bosch_range = BMA4_ACCEL_RANGE_2G;
            return true;

        case 4U:
            *bosch_range = BMA4_ACCEL_RANGE_4G;
            return true;

        case 8U:
            *bosch_range = BMA4_ACCEL_RANGE_8G;
            return true;

        case 16U:
            *bosch_range = BMA4_ACCEL_RANGE_16G;
            return true;

        default:
            return false;
    }
}

static bool MapAveraging(uint8_t averaging_samples, uint8_t *bandwidth)
{
    if (bandwidth == NULL)
    {
        return false;
    }

    switch (averaging_samples)
    {
        case 1U:
            *bandwidth = BMA4_ACCEL_OSR4_AVG1;
            return true;

        case 2U:
            *bandwidth = BMA4_ACCEL_OSR2_AVG2;
            return true;

        case 4U:
            *bandwidth = BMA4_ACCEL_NORMAL_AVG4;
            return true;

        case 8U:
            *bandwidth = BMA4_ACCEL_CIC_AVG8;
            return true;

        case 16U:
            *bandwidth = BMA4_ACCEL_RES_AVG16;
            return true;

        case 32U:
            *bandwidth = BMA4_ACCEL_RES_AVG32;
            return true;

        case 64U:
            *bandwidth = BMA4_ACCEL_RES_AVG64;
            return true;

        default:
            return false;
    }
}

bma456_motion_status_t BMA456Motion_Init(
    SPI_HandleTypeDef *hspi,
    const bma456_motion_config_t *config)
{
    struct bma4_accel_config accel_config = {0};
    uint8_t bosch_odr;
    uint8_t bosch_range;
    uint8_t bosch_bandwidth;
    int8_t result;

    if ((hspi == NULL) || (config == NULL))
    {
        return BMA456_MOTION_ERROR_PARAM;
    }

    if ((!MapOdr(config->odr, &bosch_odr)) ||
        (!MapRange(config->range_g, &bosch_range)) ||
        (!MapAveraging(config->averaging_samples, &bosch_bandwidth)))
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    s_ready = false;
    s_dev = (struct bma4_dev){0};

    s_dev.intf = BMA4_SPI_INTF;
    s_dev.bus_read = MotionSpiRead;
    s_dev.bus_write = MotionSpiWrite;
    s_dev.delay_us = MotionDelayUs;
    s_dev.intf_ptr = hspi;
    s_dev.variant = BMA45X_VARIANT;
    s_dev.read_write_len = 46U;
    s_dev.perf_mode_status = BMA4_DISABLE;

    HAL_GPIO_WritePin(Pedo_NSS_GPIO_Port, Pedo_NSS_Pin, GPIO_PIN_SET);

    result = bma456h_init(&s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_COMM;
    }

    accel_config.odr = bosch_odr;
    accel_config.bandwidth = bosch_bandwidth;
    accel_config.perf_mode = BMA4_CIC_AVG_MODE;
    accel_config.range = bosch_range;

    result = bma4_set_accel_config(&accel_config, &s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    result = bma4_set_accel_enable(BMA4_ENABLE, &s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_COMM;
    }

    /*
     * Advanced power save is part of the low-power strategy. FIFO and
     * interrupt configuration are added in the next incremental step.
     */
    result = bma4_set_advance_power_save(BMA4_ENABLE, &s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    s_range_g = config->range_g;
    s_ready = true;

    return BMA456_MOTION_OK;
}

bma456_motion_status_t BMA456Motion_ReadAccelMg(
    int16_t *x_mg,
    int16_t *y_mg,
    int16_t *z_mg)
{
    struct bma4_accel raw = {0};
    int8_t result;
    int32_t scale_numerator;

    if ((x_mg == NULL) || (y_mg == NULL) || (z_mg == NULL))
    {
        return BMA456_MOTION_ERROR_PARAM;
    }

    if (!s_ready)
    {
        return BMA456_MOTION_ERROR_NOT_READY;
    }

    result = bma4_read_accel_xyz(&raw, &s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_COMM;
    }

    /*
     * 16-bit signed full scale spans +/-range_g.
     * mg = raw * range_g * 1000 / 32768.
     */
    scale_numerator = (int32_t)s_range_g * 1000;

    *x_mg = (int16_t)(((int32_t)raw.x * scale_numerator) / 32768);
    *y_mg = (int16_t)(((int32_t)raw.y * scale_numerator) / 32768);
    *z_mg = (int16_t)(((int32_t)raw.z * scale_numerator) / 32768);

    return BMA456_MOTION_OK;
}

bool BMA456Motion_IsReady(void)
{
    return s_ready;
}
