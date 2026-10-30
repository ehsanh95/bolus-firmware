#ifndef FCNT_JOURNAL_H
#define FCNT_JOURNAL_H

#include <stdbool.h>
#include <stdint.h>

/* Two physical STM32L476 dual-bank Flash pages, 2 KiB each. */
#define FCNT_JOURNAL_PAGE_SIZE     2048U
#define FCNT_JOURNAL_SLOT_SIZE       32U
#define FCNT_JOURNAL_BLOCK_SIZE      32U

typedef struct
{
    bool (*read)(uint8_t page, uint16_t offset, uint64_t *value);
    bool (*program)(uint8_t page, uint16_t offset, uint64_t value);
    bool (*erase)(uint8_t page);
} fcnt_journal_storage_t;

typedef struct
{
    fcnt_journal_storage_t storage;
    uint32_t dev_addr;
    uint32_t reserved_high;
    uint32_t sequence;
    uint8_t active_page;
    uint8_t next_slot;
    bool ready;
} fcnt_journal_t;

/* Fails closed if a non-erased journal contains no valid record. */
bool FcntJournal_Init(fcnt_journal_t *journal,
                      const fcnt_journal_storage_t *storage,
                      uint32_t dev_addr);

/* Must return true BEFORE any new uplink can be sent. Reserves a Flash-backed
 * high-water mark; unused counters after an unexpected reset are skipped. */
bool FcntJournal_EnsureReserved(fcnt_journal_t *journal, uint32_t next_up);

#endif /* FCNT_JOURNAL_H */
