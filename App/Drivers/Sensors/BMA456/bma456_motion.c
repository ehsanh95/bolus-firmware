#include "bma456_motion.h"

#include "bma456h.h"
#include "main.h"

#define BMA456_MOTION_SPI_TIMEOUT_MS  20U
#define BMA456_MOTION_MAX_TRANSFER    64U

/*
 * Phase-5 controlled Step Counter characterization.
 * Only ODR is changed from the known-good Bosch/Phase-4 baseline.
 */
#define BMA456_STEP_CHARACTERIZATION_ODR  BMA4_OUTPUT_DATA_RATE_6_25HZ

/*
 * Phase-5 characterization diagnostics.
 *
 * These are intentionally non-static while BMA456 low-power behaviour is
 * being characterized so they can be inspected directly in CubeIDE Live
 * Expressions. They report the configuration actually read back from silicon,
 * not merely the values requested through RuntimeConfig.
 */
bool bma456_diag_readback_valid = false;
uint8_t bma456_diag_odr_code = 0U;
uint8_t bma456_diag_bandwidth_code = 0U;
uint8_t bma456_diag_perf_mode = 0U;
uint8_t bma456_diag_range_code = 0U;
uint8_t bma456_diag_range_g = 0U;
uint8_t bma456_diag_advanced_power_save = 0xFFU;

static struct bma4_dev s_dev = {0};
static bool s_ready = false;
static bool s_step_counter_enabled = false;
static uint8_t s_range_g = 2U;

/*
 * hiwdg is owned by main.c. Config-file upload is blocking enough that the
 * watchdog must be serviced while the Bosch feature engine is initialized.
 */
extern IWDG_HandleTypeDef hiwdg;

static void MotionServiceWatchdog(void)
{
    if (hiwdg.Instance == IWDG)
    {
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}

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

    MotionServiceWatchdog();

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

    MotionServiceWatchdog();

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

    MotionServiceWatchdog();

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

static bool BoschRangeToG(uint8_t bosch_range, uint8_t *range_g)
{
    if (range_g == NULL)
    {
        return false;
    }

    switch (bosch_range)
    {
        case BMA4_ACCEL_RANGE_2G:
            *range_g = 2U;
            return true;

        case BMA4_ACCEL_RANGE_4G:
            *range_g = 4U;
            return true;

        case BMA4_ACCEL_RANGE_8G:
            *range_g = 8U;
            return true;

        case BMA4_ACCEL_RANGE_16G:
            *range_g = 16U;
            return true;

        default:
            return false;
    }
}

static bool MapStepSensitivity(
    uint8_t level,
    uint16_t *param5,
    uint16_t *param13)
{
    if ((param5 == NULL) || (param13 == NULL) ||
        (level < 1U) || (level > 7U))
    {
        return false;
    }

    /*
     * Bolus calibration scale derived from Bosch guidance:
     * - Step parameter 5: 4 is most sensitive, 10 most robust.
     * - Step parameter 13: 0 is more sensitive / more false detections,
     *   1 is more robust / fewer false detections.
     *
     * Levels 1..7 are our product-level abstraction, not Bosch-defined names.
     */
    *param5 = (uint16_t)(11U - level);
    *param13 = (level >= 5U) ? 0U : 1U;

    return true;
}

static bma456_motion_status_t ConfigureStepCounter(
    bool enable,
    uint8_t sensitivity_level)
{
    struct bma456h_stepcounter_settings settings = {0};
    uint16_t param5;
    uint16_t param13;
    int8_t result;

    s_step_counter_enabled = false;

    if (!enable)
    {
        return BMA456_MOTION_OK;
    }

    if (sensitivity_level > 7U)
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    if (sensitivity_level != 0U)
    {
        if (!MapStepSensitivity(sensitivity_level, &param5, &param13))
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }

        result = bma456h_stepcounter_get_parameter(&settings, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }

        settings.param5 = param5;
        settings.param13 = param13;

        result = bma456h_stepcounter_set_parameter(&settings, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }
    }

    result = bma456h_feature_enable(
        BMA456H_STEP_COUNTER_EN,
        BMA4_ENABLE,
        &s_dev);

    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    s_step_counter_enabled = true;

    return BMA456_MOTION_OK;
}

bma456_motion_status_t BMA456Motion_Init(
    SPI_HandleTypeDef *hspi,
    const bma456_motion_config_t *config)
{
    struct bma4_accel_config accel_config = {0};
    bma456_motion_status_t step_status;
    uint8_t bosch_odr;
    uint8_t bosch_range;
    uint8_t bosch_bandwidth;
    uint8_t advanced_power_save = 0xFFU;
    int8_t result;

    if ((hspi == NULL) || (config == NULL))
    {
        return BMA456_MOTION_ERROR_PARAM;
    }

    if ((!MapOdr(config->odr, &bosch_odr)) ||
        (!MapRange(config->range_g, &bosch_range)) ||
        (!MapAveraging(config->averaging_samples, &bosch_bandwidth)) ||
        (config->step_sensitivity_level > 7U))
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    s_ready = false;
    s_step_counter_enabled = false;
    s_range_g = 2U;
    s_dev = (struct bma4_dev){0};

    bma456_diag_readback_valid = false;
    bma456_diag_odr_code = 0U;
    bma456_diag_bandwidth_code = 0U;
    bma456_diag_perf_mode = 0U;
    bma456_diag_range_code = 0U;
    bma456_diag_range_g = 0U;
    bma456_diag_advanced_power_save = 0xFFU;

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

    result = bma456h_write_config_file(&s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    /*
     * Controlled characterization path for Step Counter:
     * read the known-good Bosch baseline, modify ONLY ODR, then write the
     * complete configuration back unchanged otherwise. The baseline observed
     * on hardware was 100 Hz / AVG4 / continuous mode / +/-4 g / APS enabled.
     */
    if (config->step_counter_enable)
    {
        result = bma4_get_accel_config(&accel_config, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }

        accel_config.odr = BMA456_STEP_CHARACTERIZATION_ODR;

        result = bma4_set_accel_config(&accel_config, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }
    }
    else
    {
        accel_config.odr = bosch_odr;
        accel_config.bandwidth = bosch_bandwidth;
        accel_config.perf_mode = BMA4_CIC_AVG_MODE;
        accel_config.range = bosch_range;

        result = bma4_set_accel_config(&accel_config, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }
    }

    result = bma4_set_accel_enable(BMA4_ENABLE, &s_dev);
    if (result != BMA4_OK)
    {
        return BMA456_MOTION_ERROR_COMM;
    }

    step_status = ConfigureStepCounter(
        config->step_counter_enable,
        config->step_sensitivity_level);

    if (step_status != BMA456_MOTION_OK)
    {
        return step_status;
    }

    if (!config->step_counter_enable)
    {
        result = bma4_set_advance_power_save(BMA4_ENABLE, &s_dev);
        if (result != BMA4_OK)
        {
            return BMA456_MOTION_ERROR_CONFIG;
        }
    }

    /*
     * Read back the configuration actually active in silicon. This lets the
     * bench test confirm that only ODR changed during characterization.
     */
    result = bma4_get_accel_config(&accel_config, &s_dev);
    if ((result != BMA4_OK) ||
        (!BoschRangeToG(accel_config.range, &s_range_g)))
    {
        return BMA456_MOTION_ERROR_CONFIG;
    }

    bma456_diag_odr_code = accel_config.odr;
    bma456_diag_bandwidth_code = accel_config.bandwidth;
    bma456_diag_perf_mode = accel_config.perf_mode;
    bma456_diag_range_code = accel_config.range;
    bma456_diag_range_g = s_range_g;

    result = bma4_get_advance_power_save(
        &advanced_power_save,
        &s_dev);

    if (result == BMA4_OK)
    {
        bma456_diag_advanced_power_save = advanced_power_save;
        bma456_diag_readback_valid = true;
    }

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

    scale_numerator = (int32_t)s_range_g * 1000;

    *x_mg = (int16_t)(((int32_t)raw.x * scale_numerator) / 32768);
    *y_mg = (int16_t)(((int32_t)raw.y * scale_numerator) / 32768);
    *z_mg = (int16_t)(((int32_t)raw.z * scale_numerator) / 32768);

    return BMA456_MOTION_OK;
}

bma456_motion_status_t BMA456Motion_ReadStepCount(
    uint32_t *step_count)
{
    int8_t result;

    if (step_count == NULL)
    {
        return BMA456_MOTION_ERROR_PARAM;
    }

    if ((!s_ready) || (!s_step_counter_enabled))
    {
        return BMA456_MOTION_ERROR_NOT_READY;
    }

    result = bma456h_step_counter_output(step_count, &s_dev);

    return (result == BMA4_OK) ?
        BMA456_MOTION_OK : BMA456_MOTION_ERROR_COMM;
}

bool BMA456Motion_IsReady(void)
{
    return s_ready;
}

bool BMA456Motion_IsStepCounterEnabled(void)
{
    return s_ready && s_step_counter_enabled;
}
