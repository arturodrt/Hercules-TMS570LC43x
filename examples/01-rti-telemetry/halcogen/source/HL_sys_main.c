/** @file HL_sys_main.c 
*   @brief Application main file
*   @date 11-Dec-2018
*   @version 04.07.01
*
*   This file contains an empty main function,
*   which can be used for the application.
*/

/* 
* Copyright (C) 2009-2018 Texas Instruments Incorporated - www.ti.com  
* 
* 
*  Redistribution and use in source and binary forms, with or without 
*  modification, are permitted provided that the following conditions 
*  are met:
*
*    Redistributions of source code must retain the above copyright 
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the 
*    documentation and/or other materials provided with the   
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/


/* USER CODE BEGIN (0) */
/**
 * @file    HL_sys_main.c
 * @brief   01-rti-telemetry: a 1 kHz RTI tick, measured, streamed as CSV.
 *
 * A timer interrupt from the RTI replaces 00-blink-gio's busy-wait. The
 * interrupt measures its own latency and duration, and the main loop sends
 * the result, with the LEDs, the buttons and the CPU load, 100 times a second
 * over the XDS110 virtual COM port as CSV text lines a telemetry viewer such
 * as Radian plots live.
 *
 * Hold a user button to load the CPU and watch the telemetry respond:
 *  - button A masks IRQs for 50 us across each tick: interrupt latency rises
 *    to about 30 us;
 *  - button B does 400 us of busy work once per tick with IRQs enabled:
 *    CPU load rises, latency does not.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 *
 * @note    The application code in this file lives between the USER CODE
 *          markers, so it is preserved when HALCoGen regenerates the project.
 *          The RTI, SCI and telemetry code is in ../app.
 */

#include <math.h>

#include "app_config.h"
#include "tick.h"
#include "uart.h"
#include "telemetry.h"
#include "HL_gio.h"
#include "HL_sys_core.h"
#include "HL_sys_pmu.h"
/* USER CODE END */

/* Include Files */

#include "HL_sys_common.h"

/* USER CODE BEGIN (1) */
/*------------------------------------------------------------------------------
 * Test signal
 *----------------------------------------------------------------------------*/

/** Period of the test sine: 2 s, so 0.5 Hz. */
#define APP_SINE_PERIOD_MS  2000U

#define APP_TWO_PI          6.283185307F

/*------------------------------------------------------------------------------
 * Local function prototypes
 *----------------------------------------------------------------------------*/

/**
 * @brief  Reports whether a user button is held.
 *
 * The buttons are active low: with the board's pull-up a pin reads 1 when its
 * button is released and 0 when it is pressed.
 */
static boolean App_IsPressed(uint32 pin);

/**
 * @brief  Tick hook, called from the RTI interrupt once per millisecond.
 *
 * Toggles LED A every APP_BLINK_TICKS ticks and lights LED B while either
 * button is held. This is 00-blink-gio's loop, now driven by the timer.
 */
static void App_OnTick(uint32 tick);

/** @brief Starts the PMU cycle counter, which counts every CPU clock. */
static void App_InitCycleCounter(void);

/** @brief Busy-waits for @p cycles CPU cycles, measured by the PMU. */
static void App_SpinCycles(uint32 cycles);

/**
 * @brief  Button A: once per tick, masks IRQs for APP_CRITICAL_SECTION_US,
 *         starting APP_CRITICAL_LEAD_US before the tick is due, so the tick
 *         interrupt has to wait. Called on every pass of the main loop.
 * @return TRUE if the critical section ran.
 */
static boolean App_InjectCriticalSection(void);

/**
 * @brief  Button B: APP_BUSY_WORK_US of busy work with IRQs enabled.
 *         Called once per tick.
 * @return TRUE if the work ran.
 */
static boolean App_InjectBusyWork(void);

/** @return The test sine at @p timeMs, in thousandths. */
static sint64 App_TestSine(uint32 timeMs);

/**
 * @brief  Builds and queues one telemetry line.
 *
 * @param  sample        The tick window the line reports.
 * @param  loadPermille  CPU load over the window, in tenths of a percent.
 */
static void App_SendSample(const TickSample *sample, uint32 loadPermille);
/* USER CODE END */

/** @fn void main(void)
*   @brief Application main function
*   @note This function is empty by default.
*
*   This function is called after startup.
*   The user can use this function to implement the application.
*/

/* USER CODE BEGIN (2) */
static boolean App_IsPressed(uint32 pin)
{
    return ((APP_BUTTON_PORT->DIN & (1U << pin)) == 0U) ? TRUE : FALSE;
}

static void App_OnTick(uint32 tick)
{
    const boolean anyPressed = ((App_IsPressed(APP_BUTTON_A_PIN) == TRUE) ||
                                (App_IsPressed(APP_BUTTON_B_PIN) == TRUE)) ? TRUE : FALSE;

    gioSetBit(APP_LED_PORT, APP_LED_B_PIN, (anyPressed == TRUE) ? 1U : 0U);

    if ((tick % APP_BLINK_TICKS) == 0U)
    {
        gioToggleBit(APP_LED_PORT, APP_LED_A_PIN);
    }
}

static void App_InitCycleCounter(void)
{
    _pmuInit_();
    _pmuEnableCountersGlobal_();
    _pmuResetCycleCounter_();
    _pmuStartCounters_(pmuCYCLE_COUNTER);
}

static void App_SpinCycles(uint32 cycles)
{
    const uint32 start = _pmuGetCycleCount_();

    while ((_pmuGetCycleCount_() - start) < cycles)
    {
        /* Intentionally empty: this is the simulated work. */
    }
}

static boolean App_InjectCriticalSection(void)
{
    static uint32 s_lastTick = 0xFFFFFFFFU;
    const uint32 tick = Tick_Now();
    boolean ran = FALSE;

    /* Start only when the next tick is close: a critical section that ends
     * before the tick is due delays nothing. Once it runs, the tick count moves
     * on when the delayed interrupt is taken, so this runs once per tick. */
    if ((App_IsPressed(APP_BUTTON_A_PIN) == TRUE) &&
        (tick != s_lastTick) &&
        (Tick_UsToNext() <= APP_CRITICAL_LEAD_US))
    {
        s_lastTick = tick;

        /* A critical section that is too long: the tick interrupt cannot be
         * taken until IRQs are unmasked again. */
        _disable_IRQ_interrupt_();
        App_SpinCycles(APP_CRITICAL_SECTION_US * APP_CPU_CYCLES_PER_US);
        _enable_IRQ_interrupt_();
        ran = TRUE;
    }

    return ran;
}

static boolean App_InjectBusyWork(void)
{
    boolean ran = FALSE;

    if (App_IsPressed(APP_BUTTON_B_PIN) == TRUE)
    {
        /* Heavy work with IRQs enabled: the tick interrupts it on time. */
        App_SpinCycles(APP_BUSY_WORK_US * APP_CPU_CYCLES_PER_US);
        ran = TRUE;
    }

    return ran;
}

static sint64 App_TestSine(uint32 timeMs)
{
    /* Reduce the time to one period first, so the float keeps its precision
     * however long the board has been running. */
    const float32 phase = ((float32)(timeMs % APP_SINE_PERIOD_MS) * APP_TWO_PI)
                          / (float32)APP_SINE_PERIOD_MS;
    const float32 value = sinf(phase) * 1000.0F;

    return (sint64)((value >= 0.0F) ? (value + 0.5F) : (value - 0.5F));
}

static void App_SendSample(const TickSample *sample, uint32 loadPermille)
{
    sint64 values[TM_CHANNEL_COUNT];

    values[TM_TIME_MS]            = (sint64)sample->timeMs;
    values[TM_SEQUENCE]           = (sint64)sample->sequence;
    values[TM_LED_A]              = (sint64)((APP_LED_PORT->DOUT >> APP_LED_A_PIN) & 1U);
    values[TM_LED_B]              = (sint64)((APP_LED_PORT->DOUT >> APP_LED_B_PIN) & 1U);
    values[TM_BUTTON_A]           = (App_IsPressed(APP_BUTTON_A_PIN) == TRUE) ? 1 : 0;
    values[TM_BUTTON_B]           = (App_IsPressed(APP_BUTTON_B_PIN) == TRUE) ? 1 : 0;
    values[TM_IRQ_LATENCY_MIN_NS] = (sint64)sample->latencyMinNs;
    values[TM_IRQ_LATENCY_MAX_NS] = (sint64)sample->latencyMaxNs;
    values[TM_ISR_CYCLES_MAX]     = (sint64)sample->isrCyclesMax;
    values[TM_CPU_LOAD_PCT]       = (sint64)loadPermille;
    values[TM_TEST_SINE]          = App_TestSine(sample->timeMs);
    values[TM_LINES_DROPPED]      = (sint64)Telemetry_LinesDropped();
    values[TM_SAMPLES_MISSED]     = (sint64)Tick_MissedSamples();

    (void)Telemetry_SendSample(values);
}
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
    TickSample sample;
    boolean headerPending = TRUE;
    uint32 lastTick = 0U;
    uint32 windowStartCycles;
    uint32 windowStartIsrCycles;
    uint32 busyCycles = 0U;

    gioInit();
    gioSetDirection(APP_LED_PORT, APP_GIOB_DIRECTION);

    App_InitCycleCounter();
    Uart_Init();
    Tick_Init(&App_OnTick);
    _enable_IRQ_interrupt_();

    windowStartCycles = _pmuGetCycleCount_();
    windowStartIsrCycles = Tick_IsrCyclesTotal();

    /*
     * Super-loop. CPU load is measured here: a pass that did something counts
     * its cycles as busy, minus the tick interrupt's own cycles during the
     * pass (those are counted by the interrupt). A pass that found nothing to
     * do counts as idle. Load = (busy + interrupt cycles) / window cycles.
     */
    for (;;)
    {
        const uint32 passStartCycles = _pmuGetCycleCount_();
        const uint32 passStartIsrCycles = Tick_IsrCyclesTotal();
        const uint32 tick = Tick_Now();
        boolean busy = FALSE;
        uint8 received;

        /* Any byte from the host asks for the header again. */
        while (Uart_ReadByte(&received) == TRUE)
        {
            headerPending = TRUE;
        }

        if (headerPending == TRUE)
        {
            if (Telemetry_SendHeader() == TRUE)
            {
                headerPending = FALSE;
            }
            busy = TRUE;
        }

        if (App_InjectCriticalSection() == TRUE)
        {
            busy = TRUE;
        }

        if (tick != lastTick)
        {
            lastTick = tick;
            if (App_InjectBusyWork() == TRUE)
            {
                busy = TRUE;
            }
        }

        if (Tick_TakeSample(&sample) == TRUE)
        {
            const uint32 now = _pmuGetCycleCount_();
            const uint32 isrNow = Tick_IsrCyclesTotal();
            const uint32 windowCycles = now - windowStartCycles;
            const uint64 used = (uint64)busyCycles + (uint64)(isrNow - windowStartIsrCycles);
            uint32 loadPermille = 0U;

            if (windowCycles != 0U)
            {
                const uint64 permille = (used * 1000U) / (uint64)windowCycles;
                loadPermille = (permille > 1000U) ? 1000U : (uint32)permille;
            }

            windowStartCycles = now;
            windowStartIsrCycles = isrNow;
            busyCycles = 0U;

            App_SendSample(&sample, loadPermille);
            busy = TRUE;
        }

        if (Uart_Pump() != 0U)
        {
            busy = TRUE;
        }

        if (busy == TRUE)
        {
            const uint32 passCycles = _pmuGetCycleCount_() - passStartCycles;
            const uint32 isrCycles = Tick_IsrCyclesTotal() - passStartIsrCycles;

            busyCycles += (passCycles > isrCycles) ? (passCycles - isrCycles) : 0U;
        }
    }
/* USER CODE END */

    return 0;
}


/* USER CODE BEGIN (4) */
/* USER CODE END */
