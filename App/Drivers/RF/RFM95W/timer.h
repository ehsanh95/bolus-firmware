/*
 * ============================================================
 * timer.h
 * ============================================================
 *
 * Bolus project timer adapter for the SX1276 radio driver.
 *
 * BOLUS PROJECT CODE
 *
 * This replaces the STM32 I-CUBE-LRWAN timer-server dependency.
 * Time base: STM32 HAL tick [ms].
 *
 * Phase 5 may replace the backend for STOP-mode operation.
 * ============================================================
 */

#ifndef BOLUS_RADIO_TIMER_H
#define BOLUS_RADIO_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#include <stdint.h>
#include <stdbool.h>


typedef uint32_t TimerTime_t;

typedef void (*TimerCallback_t)(void *context);


typedef struct TimerEvent_s
{
    uint32_t Timestamp;
    uint32_t ReloadValue;

    bool IsRunning;

    TimerCallback_t Callback;
    void *Context;

    struct TimerEvent_s *Next;

} TimerEvent_t;


/*
 * Initialize a timer object.
 */
void TimerInit(
    TimerEvent_t *obj,
    TimerCallback_t callback);


/*
 * Optional callback context.
 */
void TimerSetContext(
    TimerEvent_t *obj,
    void *context);


/*
 * Set timeout period in milliseconds.
 */
void TimerSetValue(
    TimerEvent_t *obj,
    uint32_t value);


/*
 * Start timer.
 */
void TimerStart(
    TimerEvent_t *obj);


/*
 * Restart timer from now.
 */
void TimerReset(
    TimerEvent_t *obj);


/*
 * Stop timer.
 */
void TimerStop(
    TimerEvent_t *obj);


/*
 * Return true when timer is active.
 */
bool TimerIsStarted(
    TimerEvent_t *obj);


/*
 * Current HAL time in milliseconds.
 */
TimerTime_t TimerGetCurrentTime(void);


/*
 * Elapsed milliseconds since past.
 */
TimerTime_t TimerGetElapsedTime(
    TimerTime_t past);


/*
 * Cooperative timer service.
 *
 * Must be called repeatedly from main loop.
 */
void TimerProcess(void);


#ifdef __cplusplus
}
#endif

#endif /* BOLUS_RADIO_TIMER_H */
