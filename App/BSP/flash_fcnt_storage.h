#ifndef FLASH_FCNT_STORAGE_H
#define FLASH_FCNT_STORAGE_H

#include "fcnt_journal.h"

/* STM32L476RG 1 MiB bank-2 layout ONLY. Refuses other Flash densities
 * instead of risking an erase of application code. */
bool FlashFcntStorage_Create(fcnt_journal_storage_t *storage);

#endif /* FLASH_FCNT_STORAGE_H */
