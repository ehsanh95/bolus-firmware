/*
 * ============================================================
 * rfm95w_board.h
 * ============================================================
 *
 * Bolus project hardware adaptation layer
 * for HopeRF RFM95W / Semtech SX1276.
 *
 * This file is project-specific.
 *
 * The original ST/Semtech SX1276 driver is kept as intact
 * as possible. Hardware-specific functions are implemented
 * here using STM32 HAL and the Bolus PCB pinout.
 * ============================================================
 */

#ifndef RFM95W_BOARD_H
#define RFM95W_BOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#include <stdint.h>
#include <stdbool.h>

/*
 * ============================================================
 * Compile-time hardware check
 * ============================================================
 *
 * RFM95W_NSS must be configured in CubeMX as:
 *
 * PA4
 * GPIO_Output
 * Label = RFM95W_NSS
 * Initial level = HIGH
 */
#ifndef RFM95W_NSS_Pin
#error "RFM95W_NSS is not defined. Configure PA4 as GPIO_Output and label it RFM95W_NSS in CubeMX."
#endif

#ifndef RFM95W_NSS_GPIO_Port
#error "RFM95W_NSS_GPIO_Port is not defined."
#endif


/*
 * ============================================================
 * Radio switch compatibility types
 * ============================================================
 *
 * These types match the interface expected by the
 * ST SX1276 driver.
 */

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


/*
 * SX1276 DIO callback type.
 */
typedef void (DioIrqHandler)(void);


/*
 * ============================================================
 * ST/Semtech SX1276 board interface
 * ============================================================
 */

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


/*
 * ============================================================
 * Bolus diagnostic extension
 * ============================================================
 *
 * Not required by the original SX1276 driver.
 * Useful during Phase 4 bring-up.
 */

HAL_StatusTypeDef RFM95W_Board_GetLastSpiStatus(void);

/*
 * ============================================================
 * Bolus Phase 4 bring-up helper
 * ============================================================
 */
HAL_StatusTypeDef RFM95W_Board_ReadRegister(
    uint8_t address,
    uint8_t *value);


#ifdef __cplusplus
}
#endif

#endif /* RFM95W_BOARD_H */
