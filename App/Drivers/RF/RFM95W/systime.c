#include "systime.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

/*
 * Temporary LoRaWAN time backend for the communication-integration stage.
 *
 * It deliberately uses HAL_GetTick() so the LoRaMAC can be integrated before
 * STOP2/RTC work. The accumulator extends the 32-bit HAL tick across wrap as
 * long as this module is serviced at least once per HAL tick wrap period.
 * This backend is NOT the production low-power timebase.
 */
static bool s_time_initialized = false;
static uint32_t s_last_hal_tick = 0U;
static uint64_t s_monotonic_ms = 0ULL;
static int64_t s_epoch_offset_ms = 0LL;

static uint64_t BolusSysTime_MonotonicMs(void)
{
    uint32_t now = HAL_GetTick();

    if (!s_time_initialized)
    {
        s_last_hal_tick = now;
        s_monotonic_ms = (uint64_t)now;
        s_time_initialized = true;
    }
    else
    {
        uint32_t delta = now - s_last_hal_tick;
        s_last_hal_tick = now;
        s_monotonic_ms += (uint64_t)delta;
    }

    return s_monotonic_ms;
}

static SysTime_t BolusSysTime_FromSignedMs(int64_t time_ms)
{
    SysTime_t result = {0U, 0};

    if (time_ms <= 0LL)
    {
        return result;
    }

    result.Seconds = (uint32_t)((uint64_t)time_ms / 1000ULL);
    result.SubSeconds = (int16_t)((uint64_t)time_ms % 1000ULL);
    return result;
}

static int64_t BolusSysTime_ToSignedMs(SysTime_t time)
{
    return ((int64_t)time.Seconds * 1000LL) + (int64_t)time.SubSeconds;
}

SysTime_t SysTimeAdd(SysTime_t a, SysTime_t b)
{
    int64_t total_ms = BolusSysTime_ToSignedMs(a) + BolusSysTime_ToSignedMs(b);
    return BolusSysTime_FromSignedMs(total_ms);
}

SysTime_t SysTimeSub(SysTime_t a, SysTime_t b)
{
    int64_t total_ms = BolusSysTime_ToSignedMs(a) - BolusSysTime_ToSignedMs(b);
    return BolusSysTime_FromSignedMs(total_ms);
}

void SysTimeSet(SysTime_t sys_time)
{
    int64_t desired_ms = BolusSysTime_ToSignedMs(sys_time);
    int64_t monotonic_ms = (int64_t)BolusSysTime_MonotonicMs();

    s_epoch_offset_ms = desired_ms - monotonic_ms;
}

SysTime_t SysTimeGet(void)
{
    int64_t now_ms = (int64_t)BolusSysTime_MonotonicMs() + s_epoch_offset_ms;
    return BolusSysTime_FromSignedMs(now_ms);
}

SysTime_t SysTimeGetMcuTime(void)
{
    return BolusSysTime_FromSignedMs((int64_t)BolusSysTime_MonotonicMs());
}

uint32_t SysTimeToMs(SysTime_t sys_time)
{
    int64_t local_ms = BolusSysTime_ToSignedMs(sys_time) - s_epoch_offset_ms;

    if (local_ms <= 0LL)
    {
        return 0U;
    }

    return (uint32_t)local_ms;
}

SysTime_t SysTimeFromMs(uint32_t time_ms)
{
    int64_t absolute_ms = (int64_t)time_ms + s_epoch_offset_ms;
    return BolusSysTime_FromSignedMs(absolute_ms);
}
