/*
 * ============================================================
 * rfm95w_board.c
 * ============================================================
 *
 * Bolus project hardware adaptation layer
 * for HopeRF RFM95W / Semtech SX1276.
 *
 * Hardware mapping:
 * SPI=SPI1, NSS=RFM95W_NSS, RESET=RFM_RST,
 * DIO0=RFM_DIO0, DIO1=RFM_DIO1, DIO2=RFM_DIO2.
 * ============================================================
 */

#include "rfm95w_board.h"
#include "bolus_config.h"

#include <stddef.h>

extern SPI_HandleTypeDef hspi1;

#define RFM95W_PENDING_DIO0  (1UL << 0)
#define RFM95W_PENDING_DIO1  (1UL << 1)
#define RFM95W_PENDING_DIO2  (1UL << 2)

static HAL_StatusTypeDef s_rfm95w_last_spi_status = HAL_OK;
static volatile uint32_t s_rfm95w_pending_dio_mask = 0U;

static DioIrqHandler *s_rfm95w_dio_handlers[3] =
{
    NULL,
    NULL,
    NULL
};

void Sx_Board_IoInit(void)
{
    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_SET);
}

void Sx_Board_IoDeInit(void)
{
    /* Low-power GPIO reconfiguration belongs to the later power stage. */
}

void Sx_Board_IoIrqInit(DioIrqHandler **irqHandlers)
{
    if (irqHandlers == NULL)
    {
        return;
    }

    s_rfm95w_dio_handlers[0] = irqHandlers[0];
    s_rfm95w_dio_handlers[1] = irqHandlers[1];
    s_rfm95w_dio_handlers[2] = irqHandlers[2];
    s_rfm95w_pending_dio_mask = 0U;
}

uint16_t Sx_Board_SendRecv(uint16_t txData)
{
    uint8_t tx_byte = (uint8_t)(txData & 0xFFU);
    uint8_t rx_byte = 0U;

    s_rfm95w_last_spi_status =
        HAL_SPI_TransmitReceive(
            &hspi1,
            &tx_byte,
            &rx_byte,
            1U,
            RFM95W_SPI_TIMEOUT_MS);

    if (s_rfm95w_last_spi_status != HAL_OK)
    {
        return 0U;
    }

    return (uint16_t)rx_byte;
}

void Sx_Board_ChipSelect(int32_t state)
{
    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        (state == 0) ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void Sx_Board_Reset(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    gpio_init.Pin = RFM_RST_Pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RFM_RST_GPIO_Port, &gpio_init);

    HAL_GPIO_WritePin(RFM_RST_GPIO_Port, RFM_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(RFM95W_RESET_ASSERT_TIME_MS);

    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(RFM_RST_GPIO_Port, &gpio_init);
    HAL_Delay(RFM95W_RESET_RELEASE_TIME_MS);
}

uint32_t Sx_Board_GetDio1PinState(void)
{
    return (uint32_t)HAL_GPIO_ReadPin(
        RFM_DIO1_GPIO_Port,
        RFM_DIO1_Pin);
}

bool Sx_Board_CheckRfFrequency(uint32_t frequency)
{
    return ((frequency >= 862000000UL) &&
            (frequency <= 1020000000UL));
}

void Sx_Board_SetXO(uint8_t state)
{
    (void)state;
}

uint32_t Sx_Board_GetWakeUpTime(void)
{
    return 0U;
}

TxConfig_TypeDef Sx_Board_GetPaSelect(uint32_t channel)
{
    (void)channel;
    return CONF_RFO_HP;
}

void Sx_Board_SetAntSw(RfSw_TypeDef state)
{
    (void)state;
}

void Sx_Board_Bus_Init(void)
{
    HAL_GPIO_WritePin(
        RFM95W_NSS_GPIO_Port,
        RFM95W_NSS_Pin,
        GPIO_PIN_SET);
}

void Sx_Board_Bus_deInit(void)
{
    /* SPI low-power handling is intentionally deferred. */
}

HAL_StatusTypeDef RFM95W_Board_GetLastSpiStatus(void)
{
    return s_rfm95w_last_spi_status;
}

uint32_t RFM95W_Board_GetPendingIrqMask(void)
{
    return s_rfm95w_pending_dio_mask;
}

void RFM95W_Board_ProcessIrqs(void)
{
    uint32_t pending;
    uint32_t primask;

    /* Snapshot-and-clear pending DIO bits atomically. */
    primask = __get_PRIMASK();
    __disable_irq();
    pending = s_rfm95w_pending_dio_mask;
    s_rfm95w_pending_dio_mask = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }

    /*
     * Semtech DIO handlers may perform SPI register access. They are therefore
     * deliberately invoked here in cooperative main context, never from EXTI.
     */
    if (((pending & RFM95W_PENDING_DIO0) != 0U) &&
        (s_rfm95w_dio_handlers[0] != NULL))
    {
        s_rfm95w_dio_handlers[0]();
    }

    if (((pending & RFM95W_PENDING_DIO1) != 0U) &&
        (s_rfm95w_dio_handlers[1] != NULL))
    {
        s_rfm95w_dio_handlers[1]();
    }

    if (((pending & RFM95W_PENDING_DIO2) != 0U) &&
        (s_rfm95w_dio_handlers[2] != NULL))
    {
        s_rfm95w_dio_handlers[2]();
    }
}

/*
 * HAL EXTI callback: counter/flag-only ISR work. DIO handler execution is
 * deferred to RFM95W_Board_ProcessIrqs().
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == RFM_DIO0_Pin)
    {
        s_rfm95w_pending_dio_mask |= RFM95W_PENDING_DIO0;
    }
    else if (GPIO_Pin == RFM_DIO1_Pin)
    {
        s_rfm95w_pending_dio_mask |= RFM95W_PENDING_DIO1;
    }
    else if (GPIO_Pin == RFM_DIO2_Pin)
    {
        s_rfm95w_pending_dio_mask |= RFM95W_PENDING_DIO2;
    }
}

HAL_StatusTypeDef RFM95W_Board_ReadRegister(
    uint8_t address,
    uint8_t *value)
{
    uint8_t tx[2];
    uint8_t rx[2] = {0U, 0U};
    HAL_StatusTypeDef status;

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

    status = HAL_SPI_TransmitReceive(
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
