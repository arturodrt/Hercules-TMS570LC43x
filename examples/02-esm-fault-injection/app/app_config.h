/**
 * @file    app_config.h
 * @brief   02-esm-fault-injection: board pins, rates, serial and fault settings.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "HL_sys_common.h"
#include "HL_gio.h"
#include "HL_reg_sci.h"

/*------------------------------------------------------------------------------
 * Board (LAUNCHXL2-570LC43 schematic), as in 00-blink-gio
 *----------------------------------------------------------------------------*/

/** LEDs are driven by GIOB6 (LED A) and GIOB7 (LED B). */
#define APP_LED_PORT            gioPORTB
#define APP_LED_A_PIN           6U
#define APP_LED_B_PIN           7U

/** User buttons connect GIOB4 (button A) and GIOB5 (button B) to ground. */
#define APP_BUTTON_PORT         gioPORTB
#define APP_BUTTON_A_PIN        4U
#define APP_BUTTON_B_PIN        5U

/** GIOB direction mask: 1 = output, 0 = input. Only the LED pins are outputs. */
#define APP_GIOB_DIRECTION      ((1U << APP_LED_A_PIN) | (1U << APP_LED_B_PIN))

/*------------------------------------------------------------------------------
 * Tick and telemetry rates
 *----------------------------------------------------------------------------*/

/*
 * The tick itself is set in HALCoGen: RTI tab, RTI1 Compare, compare 0 from
 * counter 0 with a period of 1.000 ms. Everything below counts in those 1 ms
 * ticks, so change them together if you change that period.
 */

/** Ticks per telemetry line: 10 ticks of 1 ms give 100 lines a second. */
#define APP_TICKS_PER_SAMPLE    10U

/** LED A toggles every this many ticks: 500 ms, so it blinks at 1 Hz. */
#define APP_BLINK_TICKS         500U

/** A button must read the same for this many ticks to count as changed. */
#define APP_DEBOUNCE_TICKS      20U

/*------------------------------------------------------------------------------
 * Serial port: SCI1, 115200 8N1, set in HALCoGen's SCI1 tab (see README)
 *----------------------------------------------------------------------------*/

#define APP_SCI                 sciREG1
#define APP_TX_BUFFER_SIZE      2048U

/*------------------------------------------------------------------------------
 * Fault injection
 *----------------------------------------------------------------------------*/

/**
 * How long the ERROR pin stays active after a lockstep compare error before
 * the application releases it, so the ERR LED is visible. The ESM keeps the
 * pin low for at least its own low-time count (about 0.2 ms) in any case.
 */
#define APP_NERROR_HOLD_MS      1000U

/**
 * TRUE: after each flash ECC error is reported, its address is removed from
 * the EPC's list of known addresses, so every press of button A is reported.
 *
 * FALSE: the EPC keeps the address, and reports only the first error at it.
 * Later presses are still corrected (ecc_injected rises) but not reported
 * (ecc_detected does not). This is how the EPC stops a repeated single-bit
 * fault in a loop from flooding the ESM.
 */
#define APP_EPC_FORGET_ADDRESS  TRUE

#endif /* APP_CONFIG_H */
