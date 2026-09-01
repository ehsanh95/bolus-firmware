#ifndef BOLUS_LORAWAN_SYSTIME_H
#define BOLUS_LORAWAN_SYSTIME_H

#include <stdint.h>

typedef struct SysTime_s
{
    uint32_t Seconds;
    int16_t SubSeconds;
} SysTime_t;

SysTime_t SysTimeAdd(SysTime_t a, SysTime_t b);
SysTime_t SysTimeSub(SysTime_t a, SysTime_t b);
void SysTimeSet(SysTime_t sys_time);
SysTime_t SysTimeGet(void);
SysTime_t SysTimeGetMcuTime(void);
uint32_t SysTimeToMs(SysTime_t sys_time);
SysTime_t SysTimeFromMs(uint32_t time_ms);

#endif /* BOLUS_LORAWAN_SYSTIME_H */
