/**
 * @file    tick.c
 * @brief   1 kHz system tick from RTI compare 0, grouped into sample windows.
 *
 * The RTI is configured in HALCoGen (RTI tab, RTI1 Compare: compare 0 from
 * counter 0, period 1.000 ms; VIM channel 2 enabled as IRQ). This module calls
 * rtiInit(), enables the compare 0 notification and starts counter block 0.
 * HALCoGen's rtiCompare0Interrupt() clears the flag and calls rtiNotification(),
 * which this module supplies in place of HALCoGen's weak, empty one.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#include "tick.h"
#include "app_config.h"
#include "HL_rti.h"

/*------------------------------------------------------------------------------
 * State shared between the tick interrupt and the main loop
 *----------------------------------------------------------------------------*/

static TickHook s_hook = NULL;

static volatile uint32 s_ticks = 0U;
static volatile uint32 s_missedSamples = 0U;

/* The open window, written by the interrupt only. */
static uint32 s_windowTicks = 0U;
static uint32 s_windowSequence = 0U;

/*
 * The closed window. The interrupt writes it only while s_sampleReady is FALSE
 * and then sets the flag; the main loop reads it only while the flag is TRUE
 * and then clears it. So the two never touch it at the same time.
 */
static volatile TickSample s_sample;
static volatile boolean s_sampleReady = FALSE;

/*------------------------------------------------------------------------------
 * Tick notification, called by HALCoGen's rtiCompare0Interrupt()
 *----------------------------------------------------------------------------*/

/**
 * @brief  Replaces HALCoGen's weak rtiNotification(). Runs in the RTI compare 0
 *         IRQ, after the handler has cleared the compare 0 flag.
 */
void rtiNotification(rtiBASE_t *rtiREG, uint32 notification)
{
    uint32 tick;

    (void)rtiREG;

    if (notification == rtiNOTIFICATION_COMPARE0)
    {
        tick = s_ticks + 1U;
        s_ticks = tick;

        if (s_hook != NULL)
        {
            s_hook(tick);
        }

        s_windowTicks++;
        if (s_windowTicks >= APP_TICKS_PER_SAMPLE)
        {
            if (s_sampleReady == FALSE)
            {
                s_sample.sequence = s_windowSequence;
                s_sample.timeMs   = tick;
                s_sampleReady = TRUE;
            }
            else
            {
                /* The main loop has not taken the previous window: this one is
                 * lost, and the gap in the sequence numbers shows it. */
                s_missedSamples++;
            }

            s_windowSequence++;
            s_windowTicks = 0U;
        }
    }
}

/*------------------------------------------------------------------------------
 * Public functions
 *----------------------------------------------------------------------------*/

void Tick_Init(TickHook hook)
{
    s_hook = hook;

    rtiInit();
    rtiEnableNotification(rtiREG1, rtiNOTIFICATION_COMPARE0);
    rtiStartCounter(rtiREG1, rtiCOUNTER_BLOCK0);
}

uint32 Tick_Now(void)
{
    return s_ticks;
}

boolean Tick_TakeSample(TickSample *sample)
{
    boolean taken = FALSE;

    if (s_sampleReady == TRUE)
    {
        sample->sequence = s_sample.sequence;
        sample->timeMs   = s_sample.timeMs;
        s_sampleReady = FALSE;
        taken = TRUE;
    }

    return taken;
}

uint32 Tick_MissedSamples(void)
{
    return s_missedSamples;
}
