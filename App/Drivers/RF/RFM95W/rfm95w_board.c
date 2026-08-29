/*
 * ============================================================
 * rfm95w_board.c
 * ============================================================
 *
 * Bolus project hardware adaptation layer
 * for HopeRF RFM95W / Semtech SX1276.
 *
 * BOLUS PROJECT CODE
 *
 * Hardware mapping:
 *
 * SPI      : SPI1
 * NSS      : RFM_NSS
 * RESET    : RFM_RST
 * DIO0     : RFM_DIO0
 * DIO1     : RFM_DIO1
 * DIO2     : RFM_DIO2
 *
 * Radio power is controlled separately by bolus_power.c
 * through RFM_PWR_ON.
 * ============================================================
 */

#include "rfm95w_board.h"
#include "bolus_config.h"

#include <stddef.h>


/*
 * ============================================================
 * External CubeMX peripheral handles
 * ============================================================
 */

extern SPI_HandleTypeDef hspi1;


/*
 * ============================================================
 * Private state
 * ============================================================
 */

static HAL_StatusTypeDef s_rfm95w_last_spi_status = HAL_OK;


/*
 * Only DIO0, DIO1 and DIO2 are physically connected
 * on the current Bolus hardware.
 */
static DioIrqHandler *s_rfm95w_dio_handlers[3] =
{
    NULL,
    NULL,
    NULL
};


/*
 * ============================================================
 * IO initialization
 * ============================================================
 */

void Sx_Board_IoInit(void)
{
    /*
     * GPIO and SPI pin initialization is owned by CubeMX.
     *
     * We only guarantee that the radio is deselected
     * when no SPI transaction is active.
     */

    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_SET);
}


/*
 * ============================================================
 * IO de-initialization
 * ============================================================
 */

void Sx_Board_IoDeInit(void)
{
    /*
     * Phase 4:
     *
     * GPIO de-initialization is intentionally not performed.
     *
     * Low-power GPIO reconfiguration will be handled later
     * by the Bolus power-management layer.
     */
}


/*
 * ============================================================
 * DIO callback registration
 * ============================================================
 */

void Sx_Board_IoIrqInit(DioIrqHandler **irqHandlers)
{
    if (irqHandlers == NULL)
    {
        return;
    }

    /*
     * Current Bolus PCB exposes:
     *
     * DIO0
     * DIO1
     * DIO2
     *
     * DIO3..DIO5 are not used.
     */

    s_rfm95w_dio_handlers[0] = irqHandlers[0];
    s_rfm95w_dio_handlers[1] = irqHandlers[1];
    s_rfm95w_dio_handlers[2] = irqHandlers[2];
}


/*
 * ============================================================
 * SPI byte transfer
 * ============================================================
 */

uint16_t Sx_Board_SendRecv(uint16_t txData)
{
    uint8_t tx_byte;
    uint8_t rx_byte = 0U;

    tx_byte = (uint8_t)(txData & 0xFFU);

    s_rfm95w_last_spi_status =
        HAL_SPI_TransmitReceive(
            &hspi1,
            &tx_byte,
            &rx_byte,
            1U,
            RFM95W_SPI_TIMEOUT_MS);

    /*
     * The original SX1276 board API does not provide
     * an SPI error return value.
     *
     * Therefore the HAL status is stored separately
     * for diagnostics.
     */

    if (s_rfm95w_last_spi_status != HAL_OK)
    {
        return 0U;
    }

    return (uint16_t)rx_byte;
}


/*
 * ============================================================
 * Manual NSS / Chip Select
 * ============================================================
 */

void Sx_Board_ChipSelect(int32_t state)
{
    /*
     * state == 0  -> radio selected
     * state != 0  -> radio deselected
     */

    if (state == 0)
    {
        HAL_GPIO_WritePin(
            RFM95W_NSS_GPIO_Port,
            RFM95W_NSS_Pin,
            GPIO_PIN_RESET);
    }
    else
    {
        HAL_GPIO_WritePin(
            RFM95W_NSS_GPIO_Port,
            RFM95W_NSS_Pin,
            GPIO_PIN_SET);
    }
}


/*
 * ============================================================
 * Hardware Reset
 * ============================================================
 */

void Sx_Board_Reset(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    /*
     * Official SX1276/ST sequence:
     *
     * RESET -> output LOW
     * wait
     * RESET -> released/input
     * wait for device startup
     */

    gpio_init.Pin = RFM_RST_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        RFM_RST_GPIO_Port,
        &gpio_init);

    HAL_GPIO_WritePin(
        RFM_RST_GPIO_Port,
        RFM_RST_Pin,
        GPIO_PIN_RESET);

    HAL_Delay(
        RFM95W_RESET_ASSERT_TIME_MS);


    /*
     * Release reset instead of actively driving HIGH.
     */
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;

    HAL_GPIO_Init(
        RFM_RST_GPIO_Port,
        &gpio_init);

    HAL_Delay(
        RFM95W_RESET_RELEASE_TIME_MS);
}


/*
 * ============================================================
 * DIO1 state
 * ============================================================
 */

uint32_t Sx_Board_GetDio1PinState(void)
{
    return (uint32_t)HAL_GPIO_ReadPin(
        RFM_DIO1_GPIO_Port,
        RFM_DIO1_Pin);
}


/*
 * ============================================================
 * RF frequency check
 * ============================================================
 */

bool Sx_Board_CheckRfFrequency(uint32_t frequency)
{
    /*
     * RFM95W is the high-frequency 868/915 MHz variant.
     *
     * For the current Phase 4 driver integration we accept
     * the SX1276 upper-band operating range.
     */

    if ((frequency >= 862000000UL) &&
        (frequency <= 1020000000UL))
    {
        return true;
    }

    return false;
}


/*
 * ============================================================
 * Oscillator control
 * ============================================================
 */

void Sx_Board_SetXO(uint8_t state)
{
    /*
     * RFM95W uses its onboard crystal.
     * Bolus has no separate TCXO enable GPIO.
     */

    (void)state;
}


/*
 * ============================================================
 * Board wake-up delay
 * ============================================================
 */

uint32_t Sx_Board_GetWakeUpTime(void)
{
    /*
     * No externally controlled TCXO is used.
     */
    return 0U;
}


/*
 * ============================================================
 * PA selection
 * ============================================================
 */

TxConfig_TypeDef Sx_Board_GetPaSelect(uint32_t channel)
{
    /*
     * Current Bolus RFM95W design uses the high-power
     * PA_BOOST path.
     */

    (void)channel;

    return CONF_RFO_HP;
}


/*
 * ============================================================
 * External antenna switch control
 * ============================================================
 */

void Sx_Board_SetAntSw(RfSw_TypeDef state)
{
    /*
     * The current RFM95W module does not require a separate
     * Bolus MCU-controlled RF antenna switch GPIO.
     */

    (void)state;
}


/*
 * ============================================================
 * SPI bus initialization
 * ============================================================
 */

void Sx_Board_Bus_Init(void)
{
    /*
     * SPI1 initialization is generated and performed
     * by CubeMX before the application radio driver starts.
     *
     * Do not call MX_SPI1_Init() here because that function
     * belongs to generated application code.
     */

    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_SET);
}


/*
 * ============================================================
 * SPI bus de-initialization
 * ============================================================
 */

void Sx_Board_Bus_deInit(void)
{
    /*
     * Intentionally left active during Phase 4.
     *
     * SPI shutdown/reconfiguration belongs to the later
     * low-power state machine.
     */
}


/*
 * ============================================================
 * Bolus SPI diagnostic helper
 * ============================================================
 */

HAL_StatusTypeDef RFM95W_Board_GetLastSpiStatus(void)
{
    return s_rfm95w_last_spi_status;
}


/*
 * ============================================================
 * RFM95W EXTI dispatcher
 * ============================================================
 *
 * The project has multiple EXTI producers (RFM95W, BMA456, and later other
 * sensors), so this driver must not define the global HAL_GPIO_EXTI_Callback.
 * Core/application code owns that one HAL callback and forwards the pin here.
 */

void RFM95W_Board_HandleExti(uint16_t gpio_pin)
{
    if (gpio_pin == RFM_DIO0_Pin)
    {
        if (s_rfm95w_dio_handlers[0] != NULL)
        {
            s_rfm95w_dio_handlers[0]();
        }
    }
    else if (gpio_pin == RFM_DIO1_Pin)
    {
        if (s_rfm95w_dio_handlers[1] != NULL)
        {
            s_rfm95w_dio_handlers[1]();
        }
    }
    else if (gpio_pin == RFM_DIO2_Pin)
    {
        if (s_rfm95w_dio_handlers[2] != NULL)
        {
            s_rfm95w_dio_handlers[2]();
        }
    }
}

/*
 * ============================================================
 * Bolus Phase 4 raw register read
 * ============================================================
 *
 * SX1276 SPI read:
 *
 * bit7 = 0 -> read
 *
 * NSS LOW
 * send register address
 * send dummy byte and receive register value
 * NSS HIGH
 */

HAL_StatusTypeDef RFM95W_Board_ReadRegister(
    uint8_t address,
    uint8_t *value)
{
    uint8_t tx[2];
    uint8_t rx[2] = {0U, 0U};

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    tx[0] = address & 0x7FU;
    tx[1] = 0x00U;

    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_RESET);

    HAL_StatusTypeDef status =
        HAL_SPI_TransmitReceive(
            &hspi1,
            tx,
            rx,
            2U,
            RFM95W_SPI_TIMEOUT_MS);

    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_SET);

    if (status == HAL_OK)
    {
        *value = rx[1];
    }

    s_rfm95w_last_spi_status = status;

    return status;
}
