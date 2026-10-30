#include "fcnt_journal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define WORDS (FCNT_JOURNAL_PAGE_SIZE / 8U)

static uint64_t flash_words[2][WORDS];
static int program_count;
static int erase_count;
static int fail_program_at;
static int fail_erase;

static bool MockRead(uint8_t page, uint16_t offset, uint64_t *value)
{
    if (page >= 2U || offset >= FCNT_JOURNAL_PAGE_SIZE ||
        (offset % 8U) != 0U || value == NULL)
        return false;
    *value = flash_words[page][offset / 8U];
    return true;
}

static bool MockProgram(uint8_t page, uint16_t offset, uint64_t value)
{
    uint64_t *word;
    if (page >= 2U || offset >= FCNT_JOURNAL_PAGE_SIZE ||
        (offset % 8U) != 0U)
        return false;
    word = &flash_words[page][offset / 8U];
    program_count++;
    if (program_count == fail_program_at)
    {
        /* Imitate torn/incomplete programming from power loss. */
        *word &= (value | 0xFFFFFFFF00000000ULL);
        return false;
    }
    if ((*word & value) != value)
        return false;
    *word &= value;
    return true;
}

static bool MockErase(uint8_t page)
{
    if (page >= 2U || fail_erase)
        return false;
    memset(flash_words[page], 0xFF, sizeof(flash_words[page]));
    erase_count++;
    return true;
}

static fcnt_journal_storage_t store = {
    MockRead, MockProgram, MockErase
};

static void FreshFlash(void)
{
    memset(flash_words, 0xFF, sizeof(flash_words));
    program_count = 0;
    erase_count = 0;
    fail_program_at = 0;
    fail_erase = 0;
}

static void TestMonotonicAcrossResets(void)
{
    fcnt_journal_t a, b;
    FreshFlash();
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    assert(a.reserved_high == 0U);
    assert(FcntJournal_EnsureReserved(&a, 1U));
    assert(a.reserved_high == 32U);
    assert(program_count == 4);
    assert(FcntJournal_EnsureReserved(&a, 32U));
    assert(program_count == 4);
    assert(FcntJournal_EnsureReserved(&a, 33U));
    assert(a.reserved_high == 64U);
    assert(FcntJournal_Init(&b, &store, 0x260B1234UL));
    assert(b.reserved_high == 64U);
    assert(FcntJournal_EnsureReserved(&b, 65U));
    assert(b.reserved_high == 96U);
}

static void TestPowerCutNeverRollsBackUsedCounter(void)
{
    int step;
    for (step = 1; step <= 4; step++)
    {
        fcnt_journal_t before, after;
        FreshFlash();
        assert(FcntJournal_Init(&before, &store, 0x260B1234UL));
        assert(FcntJournal_EnsureReserved(&before, 1U));
        fail_program_at = program_count + step;
        assert(!FcntJournal_EnsureReserved(&before, 33U));
        assert(!before.ready);

        /* Interrupted reservation failed BEFORE any 33+ frame was sent. */
        assert(FcntJournal_Init(&after, &store, 0x260B1234UL));
        assert(after.reserved_high == 32U);
        fail_program_at = 0;
        assert(FcntJournal_EnsureReserved(&after, 33U));
        assert(after.reserved_high == 64U);
    }
}

static void TestPageRotationAndInterruptedErase(void)
{
    fcnt_journal_t a, b;
    unsigned i;
    FreshFlash();
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    for (i = 0; i < FCNT_JOURNAL_PAGE_SIZE / FCNT_JOURNAL_SLOT_SIZE; i++)
        assert(FcntJournal_EnsureReserved(&a, 1U + 32U * i));
    assert(a.reserved_high == 2048U && a.next_slot == 64U);

    fail_erase = 1;
    assert(!FcntJournal_EnsureReserved(&a, 2049U));
    assert(FcntJournal_Init(&b, &store, 0x260B1234UL));
    assert(b.reserved_high == 2048U);

    fail_erase = 0;
    assert(FcntJournal_EnsureReserved(&b, 2049U));
    assert(b.reserved_high == 2080U && b.active_page == 1U);
    assert(erase_count == 1);
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    assert(a.reserved_high == 2080U);

    for (i = 1U; i < FCNT_JOURNAL_PAGE_SIZE / FCNT_JOURNAL_SLOT_SIZE; i++)
        assert(FcntJournal_EnsureReserved(&a, 2049U + 32U * i));
    assert(FcntJournal_EnsureReserved(&a, 4097U));
    assert(erase_count == 2);
    assert(FcntJournal_Init(&b, &store, 0x260B1234UL));
    assert(b.reserved_high == 4128U && b.active_page == 0U);
}

static void TestInvalidDataFailsClosed(void)
{
    fcnt_journal_t a, b;
    FreshFlash();
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    assert(FcntJournal_EnsureReserved(&a, 1U));
    assert(!FcntJournal_Init(&b, &store, 0x12345678UL));
    flash_words[0][3] ^= 1ULL; /* Damaged complement of counter. */
    assert(!FcntJournal_Init(&b, &store, 0x260B1234UL));
}

static void TestPowerCutDuringPageRotation(void)
{
    fcnt_journal_t a, b;
    unsigned i;
    FreshFlash();
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    for (i = 0U; i < FCNT_JOURNAL_PAGE_SIZE / FCNT_JOURNAL_SLOT_SIZE; i++)
        assert(FcntJournal_EnsureReserved(&a, 1U + 32U * i));

    fail_program_at = program_count + 2;
    assert(!FcntJournal_EnsureReserved(&a, 2049U));
    assert(erase_count == 1);
    assert(FcntJournal_Init(&b, &store, 0x260B1234UL));
    assert(b.reserved_high == 2048U);
    fail_program_at = 0;
    assert(FcntJournal_EnsureReserved(&b, 2049U));
    assert(FcntJournal_Init(&a, &store, 0x260B1234UL));
    assert(a.reserved_high == 2080U);
}

int main(void)
{
    TestMonotonicAcrossResets();
    TestPowerCutNeverRollsBackUsedCounter();
    TestPageRotationAndInterruptedErase();
    TestPowerCutDuringPageRotation();
    TestInvalidDataFailsClosed();
    puts("PASS: power-cut-safe FCnt journal");
    return 0;
}
