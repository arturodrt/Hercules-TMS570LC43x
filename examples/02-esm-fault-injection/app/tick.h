/**
 * @file    tick.h
 * @brief   1 kHz system tick from RTI compare 0, grouped into sample windows.
 *
 * Every tick calls the application's hook. Every APP_TICKS_PER_SAMPLE ticks
 * the tick closes a window and hands it to the main loop through
 * Tick_TakeSample(), which paces the telemetry lines.
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
} TickSample;

/**
 * @brief  Initializes the RTI with HALCoGen's rtiInit() (compare 0 period set
 *         in HALCoGen: 1 ms), enables the compare 0 notification and starts
 *         counter block 0. The VIM routes the interrupt, as set in HALCoGen.
 *
 * @param  hook  Called from the tick interrupt once per tick, or NULL.
 *
 * @note   IRQs must be enabled by the caller afterwards.
 */
void Tick_Init(TickHook hook);

/** @return Ticks since Tick_Init(); one tick is 1 ms. */
uint32 Tick_Now(void);

/**
 * @brief  Takes the latest closed window, if the main loop has not taken it yet.
 *
 * @param  sample  Receives the window.
 * @return TRUE if a new window was copied into @p sample.
 */
boolean Tick_TakeSample(TickSample *sample);

/** @return Windows the interrupt closed while the previous one was still untaken. */
uint32 Tick_MissedSamples(void);

#endif /* TICK_H */
