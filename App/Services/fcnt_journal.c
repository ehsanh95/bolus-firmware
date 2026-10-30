#include "fcnt_journal.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define FCNT_MAGIC        0x31434E46UL /* 'FNC1'; layout version 1 */
#define FCNT_COMMITTED    0xA12BC34DUL
#define FCNT_SLOT_COUNT   (FCNT_JOURNAL_PAGE_SIZE / FCNT_JOURNAL_SLOT_SIZE)

typedef struct
{
    uint32_t words[8];
} fcnt_record_t;

static uint32_t RecordCrc(const uint32_t *words)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint8_t index;
    uint8_t bit;
    for (index = 0U; index < 24U; index++)
    {
        uint8_t byte = (uint8_t)(words[index / 4U] >> ((index % 4U) * 8U));
        crc ^= byte;
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc >> 1) ^ ((crc & 1UL) ? 0xEDB88320UL : 0UL);
    }
    return ~crc;
}

static bool ReadRecord(const fcnt_journal_t *journal, uint8_t page,
                       uint8_t slot, fcnt_record_t *record)
{
    uint8_t pair;
    for (pair = 0U; pair < 4U; pair++)
    {
        uint64_t value;
        uint16_t offset = (uint16_t)((uint16_t)slot * FCNT_JOURNAL_SLOT_SIZE +
                                      (uint16_t)pair * 8U);
        if (!journal->storage.read(page, offset, &value))
            return false;
        record->words[2U * pair] = (uint32_t)value;
        record->words[2U * pair + 1U] = (uint32_t)(value >> 32);
    }
    return true;
}

static bool IsErased(const fcnt_record_t *record)
{
    uint8_t i;
    for (i = 0U; i < 8U; i++)
        if (record->words[i] != 0xFFFFFFFFUL)
            return false;
    return true;
}

static bool IsValid(const fcnt_record_t *record, uint32_t dev_addr)
{
    const uint32_t *w = record->words;
    return w[0] == FCNT_MAGIC && w[1] == dev_addr &&
           w[2] != UINT32_MAX && w[3] == ~w[2] &&
           w[4] != UINT32_MAX && w[5] == ~w[4] &&
           w[6] == RecordCrc(w) && w[7] == FCNT_COMMITTED;
}

bool FcntJournal_Init(fcnt_journal_t *journal,
                      const fcnt_journal_storage_t *storage,
                      uint32_t dev_addr)
{
    bool saw_programmed = false;
    bool found = false;
    uint8_t page;

    if (journal == NULL || storage == NULL || storage->read == NULL ||
        storage->program == NULL || storage->erase == NULL)
        return false;

    memset(journal, 0, sizeof(*journal));
    journal->storage = *storage;
    journal->dev_addr = dev_addr;
    journal->active_page = 0U;

    for (page = 0U; page < 2U; page++)
    {
        uint8_t slot;
        for (slot = 0U; slot < FCNT_SLOT_COUNT; slot++)
        {
            fcnt_record_t rec;
            if (!ReadRecord(journal, page, slot, &rec))
                return false;
            if (IsErased(&rec))
                continue;
            saw_programmed = true;
            if (IsValid(&rec, dev_addr) &&
                (!found || rec.words[2] > journal->reserved_high))
            {
                found = true;
                journal->reserved_high = rec.words[2];
                journal->sequence = rec.words[4];
                journal->active_page = page;
            }
        }
    }

    /* A half-written first record (or foreign session) must not silently
     * initialize FCnt=0 and possibly violate network anti-replay protection. */
    if (!found && saw_programmed)
        return false;

    if (found)
    {
        uint8_t slot;
        journal->next_slot = FCNT_SLOT_COUNT;
        for (slot = 0U; slot < FCNT_SLOT_COUNT; slot++)
        {
            fcnt_record_t rec;
            if (!ReadRecord(journal, journal->active_page, slot, &rec))
                return false;
            if (IsErased(&rec))
            {
                journal->next_slot = slot;
                break;
            }
        }
    }
    journal->ready = true;
    return true;
}

bool FcntJournal_EnsureReserved(fcnt_journal_t *journal, uint32_t next_up)
{
    fcnt_record_t record;
    uint32_t upper;
    uint8_t page;
    uint8_t slot;
    uint8_t pair;

    if (journal == NULL || !journal->ready || next_up == 0U ||
        next_up > UINT32_MAX - FCNT_JOURNAL_BLOCK_SIZE)
        return false;
    if (next_up <= journal->reserved_high)
        return true;

    upper = (next_up + FCNT_JOURNAL_BLOCK_SIZE - 1U) &
            ~(FCNT_JOURNAL_BLOCK_SIZE - 1U);
    page = journal->active_page;
    slot = journal->next_slot;
    if (slot >= FCNT_SLOT_COUNT)
    {
        /* Erase the spare page while the last valid committed high-water
         * mark is still intact on the old page. */
        page ^= 1U;
        if (!journal->storage.erase(page))
        {
            journal->ready = false;
            return false;
        }
        slot = 0U;
    }

    record.words[0] = FCNT_MAGIC;
    record.words[1] = journal->dev_addr;
    record.words[2] = upper;
    record.words[3] = ~upper;
    record.words[4] = journal->sequence + 1U;
    record.words[5] = ~record.words[4];
    record.words[6] = RecordCrc(record.words);
    record.words[7] = FCNT_COMMITTED;

    for (pair = 0U; pair < 4U; pair++)
    {
        uint64_t value = (uint64_t)record.words[2U * pair] |
                         ((uint64_t)record.words[2U * pair + 1U] << 32);
        uint16_t offset = (uint16_t)((uint16_t)slot * FCNT_JOURNAL_SLOT_SIZE +
                                      (uint16_t)pair * 8U);
        if (!journal->storage.program(page, offset, value))
        {
            journal->ready = false;
            return false;
        }
    }

    {
        fcnt_record_t readback;
        if (!ReadRecord(journal, page, slot, &readback) ||
            !IsValid(&readback, journal->dev_addr) ||
            readback.words[2] != upper)
        {
            journal->ready = false;
            return false;
        }
    }

    journal->active_page = page;
    journal->next_slot = (uint8_t)(slot + 1U);
    journal->sequence = record.words[4];
    journal->reserved_high = upper;
    return true;
}
