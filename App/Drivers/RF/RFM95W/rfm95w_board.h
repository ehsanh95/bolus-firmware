/*
 * ============================================================
 * rfm95w_board.h
 * ============================================================
 *
 * Bolus project hardware adaptation layer
 * for HopeRF RFM95W / Semtech SX1276.
 * ============================================================
 */

#ifndef RFM95W_BOARD_H
#define RFM95W_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "bolus_config.h"

#include <stdint.h>
#include <stdbool.h>

#ifndef RFM95W_NSS_Pin
#error "RFM95W_NSS is not defined. Configure PA4 as GPIO_Output and label it RFM95W_NSS in CubeMX."
#endif

#ifndef RFM95W_NSS_GPIO_Port
#error "RFM95W_NSS_GPIO_Port is not defined."
#endif

typedef enum
{
    RFSW_OFF = 0,
    RFSW_RX,
    RFSW_RFO_LP,
    RFSW_RFO_HP,
    RFSW_RFO_LF
} RfSw_TypeDef;

typedef enum
{
    CONF_RFO_LP_HP = 0,
    CONF_RFO_LP    = 1,
    CONF_RFO_HP    = 2,
    CONF_RFO_LF    = 3
} TxConfig_TypeDef;

typedef void (DioIrqHandler)(void);

void Sx_Board_IoInit(void);
void Sx_Board_IoDeInit(void);
void Sx_Board_IoIrqInit(DioIrqHandler **irqHandlers);
uint16_t Sx_Board_SendRecv(uint16_t txData);
void Sx_Board_ChipSelect(int32_t state);
uint32_t Sx_Board_GetDio1PinState(void);
bool Sx_Board_CheckRfFrequency(uint32_t frequency);
void Sx_Board_Reset(void);
void Sx_Board_SetXO(uint8_t state);
uint32_t Sx_Board_GetWakeUpTime(void);
TxConfig_TypeDef Sx_Board_GetPaSelect(uint32_t channel);
void Sx_Board_SetAntSw(RfSw_TypeDef state);
void Sx_Board_Bus_Init(void);
void Sx_Board_Bus_deInit(void);

HAL_StatusTypeDef RFM95W_Board_GetLastSpiStatus(void);

/*
 * EXTI callback only records pending DIO bits. The actual Semtech DIO handlers
 * may read/write SX1276 registers, so they are executed later from cooperative
 * main context through this function.
 */
void RFM95W_Board_ProcessIrqs(void);
uint32_t RFM95W_Board_GetPendingIrqMask(void);

HAL_StatusTypeDef RFM95W_Board_ReadRegister(
    uint8_t address,
    uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* RFM95W_BOARD_H */
