#ifndef __UTILITIES_CONF_H__
#define __UTILITIES_CONF_H__

/* Minimal utility configuration required by the imported Semtech LoRaMAC. */
#include "cmsis_compiler.h"
#include <stdint.h>

#define UTILS_INIT_CRITICAL_SECTION()

#define UTILS_ENTER_CRITICAL_SECTION() \
    uint32_t primask_bit = __get_PRIMASK(); \
    __disable_irq()

#define UTILS_EXIT_CRITICAL_SECTION() \
    __set_PRIMASK(primask_bit)

#endif /* __UTILITIES_CONF_H__ */
