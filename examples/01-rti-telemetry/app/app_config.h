/**
 * @file    app_config.h
 * @brief   01-rti-telemetry: board pins, rates and serial settings.
 *
 * Every number the example can be tuned by lives here: the tick rate, the
 * telemetry rate, the serial port and baud rate, and the size of the load
 * that each user button injects.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "HL_sys_common.h"
#include "HL_system.h"
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
 * Clocks, from the HALCoGen clock tree (HL_system.h, in MHz)
 *----------------------------------------------------------------------------*/

/** RTI clock (RTICLK1). HALCoGen sources it from VCLK: 75 MHz. */
#define APP_RTI_CLK_HZ          ((uint32)(RTI_FREQ * 1000000.0F))

/** CPU clock (GCLK), which the PMU cycle counter counts: 300 MHz. */
#define APP_CPU_CLK_HZ          ((uint32)(GCLK_FREQ * 1000000.0F))

/** CPU cycles per microsecond, for the busy-wait helpers. */
#define APP_CPU_CYCLES_PER_US   (APP_CPU_CLK_HZ / 1000000U)

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

/*------------------------------------------------------------------------------
 * Serial port
 *----------------------------------------------------------------------------*/

/**
 * SCI module wired to the XDS110 virtual COM port. Its settings (115200 baud,
 * 8N1) are in HALCoGen's SCI1 tab. SCI1 (LIN1) has dedicated pins and needs no
 * pin multiplexing. If no text arrives on the COM port, check the LaunchPad
 * schematic; to use SCI3, enable its driver and route its pins (N2, W3) in
 * HALCoGen, regenerate, and change this to sciREG3.
 *
 * 115200 baud carries about 11.5 kB/s; a telemetry line is about 60 bytes, so
 * 100 lines a second use about half of the link. For more lines a second,
 * lower APP_TICKS_PER_SAMPLE and raise the baud rate in HALCoGen to match.
 */
#define APP_SCI                 sciREG1

/** Transmit ring buffer size in bytes. Must be a power of two. */
#define APP_TX_BUFFER_SIZE      2048U

/*------------------------------------------------------------------------------
 * Load injection (hold a user button to see it in the telemetry)
 *----------------------------------------------------------------------------*/

/**
 * Button A: once per tick, the main loop masks IRQs for this long, starting
 * APP_CRITICAL_LEAD_US before the tick is due. The tick interrupt has to wait
 * for the end of the section, so irq_lat_max_ns climbs to about
 * APP_CRITICAL_SECTION_US - APP_CRITICAL_LEAD_US: 30 us.
 */
#define APP_CRITICAL_SECTION_US 50U
#define APP_CRITICAL_LEAD_US    20U

/**
 * Button B: once per tick, the main loop does this much busy work with IRQs
 * enabled. cpu_load_pct rises by about 40 points; latency does not change.
 */
#define APP_BUSY_WORK_US        400U

#endif /* APP_CONFIG_H */
