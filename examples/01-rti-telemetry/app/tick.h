/**
 * @file    tick.h
 * @brief   1 kHz system tick from RTI compare 0, with interrupt timing.
 *
 * The tick interrupt measures two things on every tick:
 *  - its latency: the time from the RTI compare match to the first statement
 *    of the handler, read from the RTI free-running counter;
 *  - its duration in CPU cycles, read from the PMU cycle counter.
 *
 * Every APP_TICKS_PER_SAMPLE ticks it closes a window (minimum and maximum of
 * both) and hands it to the main loop through Tick_TakeSample().
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef TICK_H
#define TICK_H

#include "HL_sys_common.h"

/** Function the tick interrupt calls once per tick, with the tick count. */
typedef void (*TickHook)(uint32 tick);

/** One closed window of APP_TICKS_PER_SAMPLE ticks. */
typedef struct
{
    uint32 sequence;        /**< Window number since start; a gap means a lost window. */
    uint32 timeMs;          /**< Device time at the end of the window, in ms. */
    uint32 latencyMinNs;    /**< Shortest tick interrupt latency in the window. */
    uint32 latencyMaxNs;    /**< Longest tick interrupt latency in the window. */
    uint32 isrCyclesMax;    /**< Longest tick handler body, in CPU cycles. */
} TickSample;

/**
 * @brief  Initializes the RTI with HALCoGen's rtiInit() (compare 0 period set
 *         in HALCoGen: 1 ms), enables the compare 0 notification and starts
 *         counter block 0. The VIM routes the interrupt, as set in HALCoGen.
 *
 * @param  hook  Called from the tick interrupt once per tick, or NULL.
 *
 * @note   Requires the PMU cycle counter to be running, and IRQs to be enabled
 *         by the caller afterwards.
 */
void Tick_Init(TickHook hook);

/** @return Ticks since Tick_Init(); one tick is 1 ms. */
uint32 Tick_Now(void);

/**
 * @return Microseconds until the next tick interrupt is due. If a tick fell due
 *         while this was reading the counters, the result is about one period.
 */
uint32 Tick_UsToNext(void);

/**
 * @brief  Takes the latest closed window, if the main loop has not taken it yet.
 *
 * @param  sample  Receives the window.
 * @return TRUE if a new window was copied into @p sample.
 */
boolean Tick_TakeSample(TickSample *sample);

/**
 * @return CPU cycles spent in the tick handler body since start. It wraps;
 *         subtract two readings to get the cycles between them.
 */
uint32 Tick_IsrCyclesTotal(void);

/** @return Windows the interrupt closed while the previous one was still untaken. */
uint32 Tick_MissedSamples(void);

#endif /* TICK_H */
