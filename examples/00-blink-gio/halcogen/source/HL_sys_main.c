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
 * @brief   00-blink-gio: GPIO output and input on the LAUNCHXL2-570LC43.
 *
 * LED A blinks continuously at about 1 Hz. LED B is lit while either user
 * button is held down. The example uses only the HALCoGen GIO driver and a
 * software delay, and is the first program to run after the toolchain is set up.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-11
 *
 * @note    The application code in this file lives between the USER CODE
 *          markers, so it is preserved when HALCoGen regenerates the project.
 */

#include "HL_gio.h"
/* USER CODE END */

/* Include Files */

#include "HL_sys_common.h"

/* USER CODE BEGIN (1) */
/*------------------------------------------------------------------------------
 * Board configuration (LAUNCHXL2-570LC43 schematic)
 *----------------------------------------------------------------------------*/

/** LEDs are driven by GIOB6 (LED A) and GIOB7 (LED B). */
#define LED_PORT          gioPORTB
#define LED_A_PIN         6U
#define LED_B_PIN         7U

/** User buttons connect GIOB4 (button A) and GIOB5 (button B) to ground. */
#define BUTTON_PORT       gioPORTB
#define BUTTON_A_PIN      4U
#define BUTTON_B_PIN      5U
#define BUTTON_MASK       ((1U << BUTTON_A_PIN) | (1U << BUTTON_B_PIN))

/** GIOB direction mask: 1 = output, 0 = input. Only the LED pins are outputs. */
#define GIOB_DIRECTION    ((1U << LED_A_PIN) | (1U << LED_B_PIN))

/*------------------------------------------------------------------------------
 * Timing configuration
 *----------------------------------------------------------------------------*/

/** Busy-wait iterations per main-loop tick (about 2.5 ms; approximate). */
#define TICK_DELAY_LOOPS  50000U

/** Ticks between LED A toggles. Two toggles make one blink, about 1 Hz. */
#define BLINK_TICKS       200U

/*------------------------------------------------------------------------------
 * Local function prototypes
 *----------------------------------------------------------------------------*/

/**
 * @brief  Initializes the GIO module and configures the LED and button pins.
 *
 * Enables the GIO driver, sets the LED pins as outputs and the button pins as
 * inputs, and selects the internal pull-up resistor on the button pins.
 *
 * @note   The LaunchPad connects each button between its GIO pin and ground
 *         and provides no external pull-up. HALCoGen configures all GIO pins
 *         with a pull-down, so without the pull-up selected here a button pin
 *         would always read low.
 */
static void App_InitGpio(void);

/**
 * @brief  Reports whether a user button is currently pressed.
 *
 * The buttons are active low: with the pull-up enabled a pin reads 1 when its
 * button is released and 0 when it is pressed.
 *
 * @return TRUE if button A or button B is pressed, otherwise FALSE.
 */
static boolean App_IsButtonPressed(void);

/**
 * @brief  Blocks for an approximate time using a busy-wait loop.
 *
 * @param  loops  Number of loop iterations to execute.
 *
 * @note   The elapsed time depends on the CPU clock and the compiler
 *         optimization level. The counter is volatile so that the compiler
 *         does not remove the loop. For accurate timing use a hardware timer
 *         such as the RTI module.
 */
static void App_Delay(uint32 loops);
/* USER CODE END */

/** @fn void main(void)
*   @brief Application main function
*   @note This function is empty by default.
*
*   This function is called after startup.
*   The user can use this function to implement the application.
*/

/* USER CODE BEGIN (2) */
static void App_InitGpio(void)
{
    gioInit();
    gioSetDirection(LED_PORT, GIOB_DIRECTION);

    /* PSL selects the pull type (1 = pull-up, 0 = pull-down).
     * PULDIS disables the pull when set, so it is cleared to keep it enabled. */
    BUTTON_PORT->PSL    |= BUTTON_MASK;
    BUTTON_PORT->PULDIS &= ~BUTTON_MASK;
}

static boolean App_IsButtonPressed(void)
{
    const uint32 buttonState = BUTTON_PORT->DIN & BUTTON_MASK;

    return (buttonState != BUTTON_MASK) ? TRUE : FALSE;
}

static void App_Delay(uint32 loops)
{
    volatile uint32 count;

    for (count = 0U; count < loops; count++)
    {
        /* Intentionally empty. */
    }
}
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
    uint32 ticks = 0U;

    App_InitGpio();

    /* Cooperative super-loop: one iteration per tick. */
    for (;;)
    {
        /* Mirror the button state on LED B on every tick so that the
         * response to a button press is not tied to the blink period. */
        gioSetBit(LED_PORT, LED_B_PIN, App_IsButtonPressed() ? 1U : 0U);

        /* Toggle LED A once every BLINK_TICKS ticks. */
        ticks++;
        if (ticks >= BLINK_TICKS)
        {
            ticks = 0U;
            gioToggleBit(LED_PORT, LED_A_PIN);
        }

        App_Delay(TICK_DELAY_LOOPS);
    }
/* USER CODE END */

    return 0;
}


/* USER CODE BEGIN (4) */
/* USER CODE END */
