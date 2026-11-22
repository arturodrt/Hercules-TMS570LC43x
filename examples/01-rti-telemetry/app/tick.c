/**
 * @file    tick.c
 * @brief   1 kHz system tick from RTI compare 0, with interrupt timing.
 *
 * The RTI is configured in HALCoGen (RTI tab, RTI1 Compare: compare 0 from
 * counter 0, period 1.000 ms; VIM channel 2 enabled as IRQ). This module calls
 * rtiInit(), enables the compare 0 notification and starts counter block 0.
 * HALCoGen's rtiCompare0Interrupt() clears the flag and calls rtiNotification(),
 * which this module supplies in place of HALCoGen's weak, empty one.
 *
 * How the latency is measured: compare 0 fires when the free-running counter
 * FRC0 reaches COMP0, and at that moment the RTI adds UDCP0 to COMP0 for the
 * next tick. In the notification, COMP0 - UDCP0 is therefore the counter value
 * of the match that raised the interrupt, and FRC0 minus that value is how long
 * it took to get here: VIM dispatch, the exception entry, HALCoGen's handler up
 * to the notification call, and any time IRQs were masked. FRC0 advances once
 * every CPUC0 + 1 RTI clocks; with HALCoGen's default prescaler of 8 that is
 * 9.375 MHz, a resolution of 107 ns.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#include "tick.h"
#include "app_config.h"
#include "HL_rti.h"
#include "HL_sys_pmu.h"

/*------------------------------------------------------------------------------
 * State shared between the tick interrupt and the main loop
 *----------------------------------------------------------------------------*/

static TickHook s_hook = NULL;

/** RTI clocks per FRC0 count (CPUC0 + 1), read from the HALCoGen setup. */
static uint32 s_prescale = 1U;

static volatile uint32 s_ticks = 0U;
static volatile uint32 s_isrCyclesTotal = 0U;
static volatile uint32 s_missedSamples = 0U;

/* The open window, written by the interrupt only. Latencies are in RTI counts. */
static uint32 s_windowTicks = 0U;
static uint32 s_windowSequence = 0U;
static uint32 s_windowLatencyMin = 0xFFFFFFFFU;
static uint32 s_windowLatencyMax = 0U;
static uint32 s_windowCyclesMax = 0U;

/*
 * The closed window, latencies still in RTI counts; the main loop converts
 * them, so the interrupt does no division. The interrupt writes it only while
 * s_sampleReady is FALSE and then sets the flag; the main loop reads it only
 * while the flag is TRUE and then clears it. So the two never touch it at the
 * same time.
 */
typedef struct
{
    uint32 sequence;
    uint32 timeMs;
    uint32 latencyMinCounts;
    uint32 latencyMaxCounts;
    uint32 isrCyclesMax;
} TickWindow;

static volatile TickWindow s_sample;
static volatile boolean s_sampleReady = FALSE;

/*------------------------------------------------------------------------------
 * Local functions
 *----------------------------------------------------------------------------*/

/** Converts FRC0 counts to nanoseconds. */
static uint32 Tick_CountsToNs(uint32 counts)
{
    const uint64 ns = ((uint64)counts * (uint64)s_prescale * 1000000000ULL)
                      / (uint64)APP_RTI_CLK_HZ;

    return (ns > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32)ns;
}

/** Closes the open window: hands it to the main loop and starts a new one. */
static void Tick_CloseWindow(uint32 now)
{
    if (s_sampleReady == FALSE)
    {
        s_sample.sequence         = s_windowSequence;
        s_sample.timeMs           = now;
        s_sample.latencyMinCounts = s_windowLatencyMin;
        s_sample.latencyMaxCounts = s_windowLatencyMax;
        s_sample.isrCyclesMax     = s_windowCyclesMax;
        s_sampleReady = TRUE;
    }
    else
    {
        /* The main loop has not taken the previous window: this one is lost,
         * and the gap in the sequence numbers shows it on the host. */
        s_missedSamples++;
    }

    s_windowSequence++;
    s_windowTicks = 0U;
    s_windowLatencyMin = 0xFFFFFFFFU;
    s_windowLatencyMax = 0U;
    s_windowCyclesMax = 0U;
}

/*------------------------------------------------------------------------------
 * Tick notification, called by HALCoGen's rtiCompare0Interrupt()
 *----------------------------------------------------------------------------*/

/**
 * @brief  Replaces HALCoGen's weak rtiNotification(). Runs in the RTI compare 0
 *         IRQ, after the handler has cleared the compare 0 flag.
 */
void rtiNotification(rtiBASE_t *rtiREG, uint32 notification)
{
    /* Read the clocks first, so the measurement includes as little of this
     * function as possible. */
    const uint32 startCycles = _pmuGetCycleCount_();
    const uint32 nowCounts   = rtiREG->CNT[0U].FRCx;
    const uint32 matchCounts = rtiREG->CMP[0U].COMPx - rtiREG->CMP[0U].UDCPx;
    const uint32 latency     = nowCounts - matchCounts;
    uint32 tick;
    uint32 cycles;

    if (notification == rtiNOTIFICATION_COMPARE0)
    {
        tick = s_ticks + 1U;
        s_ticks = tick;

        if (s_hook != NULL)
        {
            s_hook(tick);
        }

        if (latency < s_windowLatencyMin)
        {
            s_windowLatencyMin = latency;
        }
        if (latency > s_windowLatencyMax)
        {
            s_windowLatencyMax = latency;
        }

        s_windowTicks++;
        if (s_windowTicks >= APP_TICKS_PER_SAMPLE)
        {
            Tick_CloseWindow(tick);
        }

        /* The notification's own cost, up to here. It counts towards the next
         * window when this tick closed the current one. */
        cycles = _pmuGetCycleCount_() - startCycles;
        s_isrCyclesTotal += cycles;
        if (cycles > s_windowCyclesMax)
        {
            s_windowCyclesMax = cycles;
        }
    }
}

/*------------------------------------------------------------------------------
 * Public functions
 *----------------------------------------------------------------------------*/

void Tick_Init(TickHook hook)
{
    s_hook = hook;

    /* Counters stopped, compare 0 loaded with the HALCoGen period. */
    rtiInit();
    s_prescale = rtiREG1->CNT[0U].CPUCx + 1U;

    rtiEnableNotification(rtiREG1, rtiNOTIFICATION_COMPARE0);
    rtiStartCounter(rtiREG1, rtiCOUNTER_BLOCK0);
}

uint32 Tick_Now(void)
{
    return s_ticks;
}

uint32 Tick_UsToNext(void)
{
    /* FRC0 first: if the match happens between the two reads, COMP0 has
     * already moved on by a period, and the result is about one period
     * rather than a wrapped, huge number. */
    const uint32 now  = rtiREG1->CNT[0U].FRCx;
    const uint32 next = rtiREG1->CMP[0U].COMPx;
    const uint32 counts = next - now;
    const uint64 us = ((uint64)counts * (uint64)s_prescale * 1000000ULL)
                      / (uint64)APP_RTI_CLK_HZ;

    return (us > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (uint32)us;
}

boolean Tick_TakeSample(TickSample *sample)
{
    boolean taken = FALSE;

    if (s_sampleReady == TRUE)
    {
        sample->sequence     = s_sample.sequence;
        sample->timeMs       = s_sample.timeMs;
        sample->latencyMinNs = Tick_CountsToNs(s_sample.latencyMinCounts);
        sample->latencyMaxNs = Tick_CountsToNs(s_sample.latencyMaxCounts);
        sample->isrCyclesMax = s_sample.isrCyclesMax;
        s_sampleReady = FALSE;
        taken = TRUE;
    }

    return taken;
}

uint32 Tick_IsrCyclesTotal(void)
{
    return s_isrCyclesTotal;
}

uint32 Tick_MissedSamples(void)
{
    return s_missedSamples;
}
