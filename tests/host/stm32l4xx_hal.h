#ifndef TEST_HAL_H
#define TEST_HAL_H
#include <stdint.h>

typedef struct { int unused; } SPI_HandleTypeDef;
typedef struct { int unused; } I2C_HandleTypeDef;

uint32_t HAL_GetTick(void);
#endif
