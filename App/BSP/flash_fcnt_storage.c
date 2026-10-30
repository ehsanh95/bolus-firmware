#include "flash_fcnt_storage.h"
#include "main.h"

#include <stddef.h>

#define FCNT_FLASH_FIRST_PAGE_ADDR  0x080FF000UL
#define FCNT_FLASH_LAST_ADDR        0x080FFFFFUL
#define FCNT_FLASH_TOTAL_KIB       1024U

static bool AddressFor(uint8_t page, uint16_t offset, uint32_t *address)
{
    if (address == NULL || page >= 2U ||
        offset >= FCNT_JOURNAL_PAGE_SIZE || (offset % 8U) != 0U)
        return false;
    *address = FCNT_FLASH_FIRST_PAGE_ADDR +
               (uint32_t)page * FCNT_JOURNAL_PAGE_SIZE + offset;
    return (*address <= FCNT_FLASH_LAST_ADDR - 7U);
}

static bool ReadDoubleword(uint8_t page, uint16_t offset, uint64_t *value)
{
    uint32_t address;
    if (value == NULL || !AddressFor(page, offset, &address))
        return false;
    *value = *(volatile const uint64_t *)(uintptr_t)address;
    return true;
}

static bool ProgramDoubleword(uint8_t page, uint16_t offset, uint64_t value)
{
    uint32_t address;
    HAL_StatusTypeDef status;
    if (!AddressFor(page, offset, &address) ||
        HAL_FLASH_Unlock() != HAL_OK)
        return false;

    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, value);
    (void)HAL_FLASH_Lock();
    return (status == HAL_OK &&
            *(volatile const uint64_t *)(uintptr_t)address == value);
}

static bool ErasePage(uint8_t page)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error_page = 0xFFFFFFFFUL;
    HAL_StatusTypeDef status;

    if (page >= 2U || HAL_FLASH_Unlock() != HAL_OK)
        return false;

    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_2;
    erase.Page = 254U + page;
    erase.NbPages = 1U;
    status = HAL_FLASHEx_Erase(&erase, &error_page);
    (void)HAL_FLASH_Lock();
    return (status == HAL_OK && error_page == 0xFFFFFFFFUL);
}

bool FlashFcntStorage_Create(fcnt_journal_storage_t *storage)
{
    uint16_t size_kib;
    if (storage == NULL)
        return false;

    size_kib = *(volatile const uint16_t *)(uintptr_t)FLASH_SIZE_DATA_REGISTER;
    size_kib &= 0x0FFFU;
    if (size_kib != FCNT_FLASH_TOTAL_KIB ||
        FLASH_PAGE_SIZE != FCNT_JOURNAL_PAGE_SIZE)
        return false;

    storage->read = ReadDoubleword;
    storage->program = ProgramDoubleword;
    storage->erase = ErasePage;
    return true;
}
