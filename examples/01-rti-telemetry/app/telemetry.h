/**
 * @file    telemetry.h
 * @brief   Telemetry as CSV text lines: one header line, then one line per sample.
 *
 * Wire format (what a host reads):
 *
 *     t_ms,seq,led_a,led_b,btn_a,btn_b,irq_lat_min_ns,irq_lat_max_ns,...\r\n
 *     20,1,0,0,0,0,453,480,...\r\n
 *
 *  - Fields are separated by commas; lines end in CR LF.
 *  - The first field is device time in milliseconds since reset.
 *  - The header is sent once after reset, and again whenever the board
 *    receives any byte, so a host that connects late can ask for it.
 *  - Nothing else is ever printed: no banner, no log lines.
 *
 * The channel list is TelemetryChannel; the names and decimals for each one
 * are in one table in telemetry.c, so the header and the data lines cannot
 * disagree.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "HL_sys_common.h"

/** Telemetry channels, in the order they appear on a line. */
typedef enum
{
    TM_TIME_MS = 0,         /**< t_ms: device time at the end of the sample window */
    TM_SEQUENCE,            /**< seq: sample number; a gap means a lost line       */
    TM_LED_A,               /**< led_a: 1 when LED A is lit                        */
    TM_LED_B,               /**< led_b: 1 when LED B is lit                        */
    TM_BUTTON_A,            /**< btn_a: 1 while button A is held                   */
    TM_BUTTON_B,            /**< btn_b: 1 while button B is held                   */
    TM_IRQ_LATENCY_MIN_NS,  /**< irq_lat_min_ns: shortest tick latency in window   */
    TM_IRQ_LATENCY_MAX_NS,  /**< irq_lat_max_ns: longest tick latency in window    */
    TM_ISR_CYCLES_MAX,      /**< isr_cycles_max: longest tick handler, CPU cycles  */
    TM_CPU_LOAD_PCT,        /**< cpu_load_pct: busy share of the CPU, one decimal  */
    TM_TEST_SINE,           /**< test_sine: 0.5 Hz sine from the FPU, 3 decimals   */
    TM_LINES_DROPPED,       /**< lines_dropped: lines lost to a full TX buffer     */
    TM_SAMPLES_MISSED,      /**< samples_missed: windows the main loop missed      */
    TM_CHANNEL_COUNT
} TelemetryChannel;

/**
 * @brief  Queues the header line.
 * @return TRUE if it was queued, FALSE if the transmit buffer was full.
 */
boolean Telemetry_SendHeader(void);

/**
 * @brief  Formats and queues one data line.
 *
 * @param  values  One value per channel, indexed by TelemetryChannel. A channel
 *                 with N decimals takes its value scaled by 10^N: 12.3 % is 123.
 * @return TRUE if the line was queued, FALSE if the transmit buffer was full
 *         (the line is dropped and counted).
 */
boolean Telemetry_SendSample(const sint64 values[TM_CHANNEL_COUNT]);

/** @return Data lines dropped because the transmit buffer was full. */
uint32 Telemetry_LinesDropped(void);

#endif /* TELEMETRY_H */
