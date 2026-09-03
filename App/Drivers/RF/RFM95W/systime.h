#ifndef BOLUS_LORAWAN_SYSTIME_H
#define BOLUS_LORAWAN_SYSTIME_H

#include <stdint.h>

/*
 * Number of seconds between the Unix epoch (1970-01-01) and GPS epoch
 * (1980-01-06). LoRaMAC uses this when processing DeviceTimeAns.
 * Keep this value aligned with the I-CUBE-LRWAN systime contract.
 */
#define UNIX_GPS_EPOCH_OFFSET ((uint32_t)315964800U)

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
