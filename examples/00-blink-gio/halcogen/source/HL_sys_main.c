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
#include "HL_gio.h"
/* USER CODE END */

/* Include Files */

#include "HL_sys_common.h"

/* USER CODE BEGIN (1) */
/*
 * 00-blink-gio: GIO output and input on the LAUNCHXL2-570LC43.
 *
 * LED A blinks continuously. LED B is lit while user button B is held.
 * The user buttons pull their pins low when pressed.
 */
#define LED_PORT         gioPORTB
#define LED_A_PIN        6U
#define LED_B_PIN        7U

#define BUTTON_PORT      gioPORTB
#define BUTTON_B_PIN     5U

/* GIOB pin direction mask: 1 = output, 0 = input. */
#define GIOB_DIRECTION   ((1U << LED_A_PIN) | (1U << LED_B_PIN))

/* The main loop runs one tick per TICK_DELAY_LOOPS busy-wait iterations and
 * toggles LED A every BLINK_TICKS ticks. Timing is approximate: it depends on
 * the CPU clock and the compiler optimization level. */
#define TICK_DELAY_LOOPS 50000U
#define BLINK_TICKS      100U
/* USER CODE END */

/** @fn void main(void)
*   @brief Application main function
*   @note This function is empty by default.
*
*   This function is called after startup.
*   The user can use this function to implement the application.
*/

/* USER CODE BEGIN (2) */
static void delay(uint32 loops)
{
    volatile uint32 count;

    for (count = 0U; count < loops; count++)
    {
    }
}
/* USER CODE END */

int main(void)
{
/* USER CODE BEGIN (3) */
    gioInit();
    gioSetDirection(LED_PORT, GIOB_DIRECTION);

    uint32 ticks = 0U;

    for (;;)
    {
        /* Button is active low: a pressed button reads 0. */
        gioSetBit(LED_PORT, LED_B_PIN,
                  (gioGetBit(BUTTON_PORT, BUTTON_B_PIN) == 0U) ? 1U : 0U);

        if (++ticks >= BLINK_TICKS)
        {
            ticks = 0U;
            gioToggleBit(LED_PORT, LED_A_PIN);
        }

        delay(TICK_DELAY_LOOPS);
    }
/* USER CODE END */

    return 0;
}


/* USER CODE BEGIN (4) */
/* USER CODE END */
