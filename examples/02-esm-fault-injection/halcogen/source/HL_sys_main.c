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
 * @brief   02-esm-fault-injection: inject real faults, watch the ESM report them.
 *
 *  - Button A: one flash single-bit ECC error. The CPU corrects it, the EPC
 *    records it, the ESM flags it (group 1). LED B toggles on each report.
 *  - Button B: one lockstep compare error. The ESM flags it (group 2),
 *    interrupts with an FIQ and drives the ERROR pin low: the ERR LED lights
 *    for APP_NERROR_HOLD_MS.
 *
 * LED A blinks at 1 Hz from the RTI tick, as in 01-rti-telemetry, so you can
 * see the program keep running through every fault. The counters stream over
 * the XDS110 COM port as CSV lines, 100 a second.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 *
 * @note    The application code in this file lives between the USER CODE
 *          markers, so it is preserved when HALCoGen regenerates the project.
 *          The fault, RTI, SCI and telemetry code is in ../app.
 */

#include "app_config.h"
#include "fault.h"
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
 * Button debouncing, done in the tick interrupt
 *----------------------------------------------------------------------------*/

/** Debounce state of one button. */
typedef struct
{
    uint32  pin;            /**< GIOB pin of the button. */
    uint32  count;          /**< Ticks the raw state has differed from stable. */
    boolean stable;         /**< Debounced state: TRUE = pressed. */
    volatile boolean press; /**< Set on each press; cleared by the main loop. */
} Button;

static Button s_buttonA = { APP_BUTTON_A_PIN, 0U, FALSE, FALSE };
static Button s_buttonB = { APP_BUTTON_B_PIN, 0U, FALSE, FALSE };

/*------------------------------------------------------------------------------
 * Local function prototypes
 *----------------------------------------------------------------------------*/

/**
 * @brief  Reports whether a user button is held (raw, not debounced).
 *
 * The buttons are active low: with the board's pull-up a pin reads 1 when its
 * button is released and 0 when it is pressed.
 */
static boolean App_IsPressed(uint32 pin);

/** @brief Debounces one button; sets its press flag on a debounced press. */
static void App_Debounce(Button *button);

/**
 * @brief  Tick hook, called from the RTI interrupt once per millisecond:
 *         blinks LED A and debounces both buttons.
 */
static void App_OnTick(uint32 tick);

/** @brief Starts the PMU cycle counter, which counts every CPU clock. */
static void App_InitCycleCounter(void);

/** @return TRUE once if @p button was pressed since the last call. */
static boolean App_TakePress(Button *button);

/** @brief Builds and queues one telemetry line. */
static void App_SendSample(const TickSample *sample, const FaultStatus *fault);
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

static void App_Debounce(Button *button)
{
    const boolean raw = App_IsPressed(button->pin);

    if (raw == button->stable)
    {
        button->count = 0U;
    }
    else
    {
        button->count++;
        if (button->count >= APP_DEBOUNCE_TICKS)
        {
            button->stable = raw;
            button->count = 0U;
            if (raw == TRUE)
            {
                button->press = TRUE;
            }
        }
    }
}

static void App_OnTick(uint32 tick)
{
    if ((tick % APP_BLINK_TICKS) == 0U)
    {
        gioToggleBit(APP_LED_PORT, APP_LED_A_PIN);
    }

    App_Debounce(&s_buttonA);
    App_Debounce(&s_buttonB);
}

static void App_InitCycleCounter(void)
{
    _pmuInit_();
    _pmuEnableCountersGlobal_();
    _pmuResetCycleCounter_();
    _pmuStartCounters_(pmuCYCLE_COUNTER);
}

static boolean App_TakePress(Button *button)
{
    boolean pressed = FALSE;

    /* Only the tick interrupt sets the flag and only this clears it. Two
     * debounced presses are at least 2 * APP_DEBOUNCE_TICKS apart, far longer
     * than the gap between this read and the clear, so none is lost. */
    if (button->press == TRUE)
    {
        button->press = FALSE;
        pressed = TRUE;
    }

    return pressed;
}

static void App_SendSample(const TickSample *sample, const FaultStatus *fault)
{
    sint64 values[TM_CHANNEL_COUNT];

    values[TM_TIME_MS]            = (sint64)sample->timeMs;
    values[TM_SEQUENCE]           = (sint64)sample->sequence;
    values[TM_BUTTON_A]           = (App_IsPressed(APP_BUTTON_A_PIN) == TRUE) ? 1 : 0;
    values[TM_BUTTON_B]           = (App_IsPressed(APP_BUTTON_B_PIN) == TRUE) ? 1 : 0;
    values[TM_NERROR]             = (fault->nErrorActive == TRUE) ? 1 : 0;
    values[TM_ECC_INJECTED]       = (sint64)fault->eccInjected;
    values[TM_ECC_DETECTED]       = (sint64)fault->eccDetected;
    values[TM_ECC_READ_OK]        = (fault->eccReadOk == TRUE) ? 1 : 0;
    values[TM_EPC_CAM_USED]       = (sint64)fault->epcCamUsed;
    values[TM_CCM_INJECTED]       = (sint64)fault->ccmInjected;
    values[TM_CCM_DETECTED]       = (sint64)fault->ccmDetected;
    values[TM_CCM_SELFTEST_FLAGS] = (sint64)fault->ccmSelfTestFlags;
    values[TM_CCM_DETECT_CYCLES]  = (sint64)fault->ccmDetectCycles;
    values[TM_ESM_UNEXPECTED]     = (sint64)fault->unexpected;
    values[TM_ESM_BOOT_G1]        = (sint64)fault->bootGroup1;
    values[TM_ESM_BOOT_G2]        = (sint64)fault->bootGroup2;
    values[TM_ESM_UNEXPECTED_CH]  = (sint64)fault->unexpectedChannels;
    values[TM_EPC_FIRST_ADDR]     = (sint64)fault->epcFirstAddress;
    values[TM_LINES_DROPPED]      = (sint64)Telemetry_LinesDropped();
    values[TM_SAMPLES_MISSED]     = (sint64)Tick_MissedSamples();

    (void)Telemetry_SendSample(values);
}
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
    static const uint8 k_lineEnd[2] = { (uint8)'\r', (uint8)'\n' };
    TickSample sample;
    FaultStatus fault;
    boolean headerPending = TRUE;
    uint32 lastEccDetected = 0U;

    gioInit();
    gioSetDirection(APP_LED_PORT, APP_GIOB_DIRECTION);

    App_InitCycleCounter();
    Uart_Init();

    /* A reset can cut the last line short, and the pin can glitch while the
     * SCI starts; end whatever the host has so far, so the header that follows
     * starts a line of its own. */
    (void)Uart_Write(k_lineEnd, (uint32)sizeof(k_lineEnd));

    (void)Fault_Init();
    Tick_Init(&App_OnTick);

    /* IRQ for the tick, FIQ for the ESM's high-level interrupt. */
    _enable_interrupt_();

    for (;;)
    {
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
        }

        if (App_TakePress(&s_buttonA) == TRUE)
        {
            Fault_InjectFlashEcc();
        }

        if (App_TakePress(&s_buttonB) == TRUE)
        {
            Fault_InjectLockstepError();
        }

        Fault_Poll(Tick_Now());
        Fault_GetStatus(&fault);

        /* LED B toggles on every reported ECC error, as in TI's demo. */
        if (fault.eccDetected != lastEccDetected)
        {
            lastEccDetected = fault.eccDetected;
            gioToggleBit(APP_LED_PORT, APP_LED_B_PIN);
        }

        if (Tick_TakeSample(&sample) == TRUE)
        {
            App_SendSample(&sample, &fault);
        }

        (void)Uart_Pump();
    }
/* USER CODE END */

    return 0;
}


/* USER CODE BEGIN (4) */
/* USER CODE END */
