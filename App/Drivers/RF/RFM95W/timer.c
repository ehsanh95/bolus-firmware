/*
 * ============================================================
 * timer.c
 * ============================================================
 *
 * Bolus project cooperative timer implementation
 * for the SX1276 radio driver.
 *
 * BOLUS PROJECT CODE
 *
 * Time source:
 *     HAL_GetTick()
 *
 * Resolution:
 *     1 ms
 *
 * This implementation intentionally avoids the complete
 * I-CUBE-LRWAN timer server dependency.
 * ============================================================
 */

#include "timer.h"

#include <stddef.h>


/*
 * ============================================================
 * Private timer list
 * ============================================================
 */

static TimerEvent_t *s_timer_list = NULL;


/*
 * ============================================================
 * TimerInit
 * ============================================================
 */

void TimerInit(
    TimerEvent_t *obj,
    TimerCallback_t callback)
{
    TimerEvent_t *node;

    if (obj == NULL)
    {
        return;
    }

    obj->Timestamp = 0U;
    obj->ReloadValue = 0U;
    obj->IsRunning = false;

    obj->Callback = callback;
    obj->Context = NULL;
    obj->Next = NULL;


    /*
     * Avoid registering the same timer twice.
     */
    node = s_timer_list;

    while (node != NULL)
    {
        if (node == obj)
        {
            return;
        }

        node = node->Next;
    }


    /*
     * Insert timer at list head.
     */
    obj->Next = s_timer_list;
    s_timer_list = obj;
}


/*
 * ============================================================
 * TimerSetContext
 * ============================================================
 */

void TimerSetContext(
    TimerEvent_t *obj,
    void *context)
{
    if (obj == NULL)
    {
        return;
    }

    obj->Context = context;
}


/*
 * ============================================================
 * TimerSetValue
 * ============================================================
 */

void TimerSetValue(
    TimerEvent_t *obj,
    uint32_t value)
{
    if (obj == NULL)
    {
        return;
    }

    /*
     * Zero-duration timers are forced to 1 ms.
     */
    obj->ReloadValue =
        (value == 0U) ? 1U : value;
}


/*
 * ============================================================
 * TimerStart
 * ============================================================
 */

void TimerStart(
    TimerEvent_t *obj)
{
    if (obj == NULL)
    {
        return;
    }

    obj->Timestamp = HAL_GetTick();
    obj->IsRunning = true;
}


/*
 * ============================================================
 * TimerReset
 * ============================================================
 */

void TimerReset(
    TimerEvent_t *obj)
{
    TimerStart(obj);
}


/*
 * ============================================================
 * TimerStop
 * ============================================================
 */

void TimerStop(
    TimerEvent_t *obj)
{
    if (obj == NULL)
    {
        return;
    }

    obj->IsRunning = false;
}


/*
 * ============================================================
 * TimerIsStarted
 * ============================================================
 */

bool TimerIsStarted(
    TimerEvent_t *obj)
{
    if (obj == NULL)
    {
        return false;
    }

    return obj->IsRunning;
}


/*
 * ============================================================
 * Time helpers
 * ============================================================
 */

TimerTime_t TimerGetCurrentTime(void)
{
    return HAL_GetTick();
}


TimerTime_t TimerGetElapsedTime(
    TimerTime_t past)
{
    /*
     * Unsigned subtraction also handles HAL tick rollover.
     */
    return HAL_GetTick() - past;
}


/*
 * ============================================================
 * TimerProcess
 * ============================================================
 */

void TimerProcess(void)
{
    TimerEvent_t *node;
    uint32_t now;

    now = HAL_GetTick();
    node = s_timer_list;


    while (node != NULL)
    {
        if (node->IsRunning)
        {
            /*
             * Signed subtraction makes rollover-safe
             * deadline comparison possible.
             */
            if ((int32_t)
                (now -
                 (node->Timestamp +
                  node->ReloadValue)) >= 0)
            {
                /*
                 * Mark stopped BEFORE callback.
                 *
                 * Callback is allowed to restart
                 * the same timer.
                 */
                node->IsRunning = false;

                if (node->Callback != NULL)
                {
                    node->Callback(
                        node->Context);
                }
            }
        }

        node = node->Next;
    }
}
