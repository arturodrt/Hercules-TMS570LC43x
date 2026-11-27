/**
 * @file    telemetry.h
 * @brief   Telemetry as CSV text lines: one header line, then one line per sample.
 *
 * Wire format (what a host reads):
 *
 *     t_ms,seq,btn_a,btn_b,nerror,ecc_injected,ecc_detected,...\r\n
 *     20,1,0,0,0,0,0,...\r\n
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
    TM_TIME_MS = 0,         /**< t_ms: device time at the end of the sample window  */
    TM_SEQUENCE,            /**< seq: sample number; a gap means a lost line        */
    TM_BUTTON_A,            /**< btn_a: 1 while button A is held                    */
    TM_BUTTON_B,            /**< btn_b: 1 while button B is held                    */
    TM_NERROR,              /**< nerror: 1 while the ESM drives the ERROR pin low   */
    TM_ECC_INJECTED,        /**< ecc_injected: flash single-bit errors injected     */
    TM_ECC_DETECTED,        /**< ecc_detected: EPC correctable errors (ESM 1.4)     */
    TM_ECC_READ_OK,         /**< ecc_read_ok: 1 if the last bad read came back right */
    TM_EPC_CAM_USED,        /**< epc_cam_used: addresses the EPC remembers (of 32)  */
    TM_CCM_INJECTED,        /**< ccm_injected: lockstep compare errors forced       */
    TM_CCM_DETECTED,        /**< ccm_detected: CPU compare errors (ESM 2.2)         */
    TM_CCM_SELFTEST_FLAGS,  /**< ccm_selftest_flags: second-path flags (ESM 1.31)   */
    TM_CCM_DETECT_CYCLES,   /**< ccm_detect_cycles: forcing to ESM handler, cycles  */
    TM_ESM_UNEXPECTED,      /**< esm_unexpected: group 2 errors not injected here   */
    TM_ESM_BOOT_G1,         /**< esm_boot_g1: group 1 flags left at start-up (bits) */
    TM_ESM_BOOT_G2,         /**< esm_boot_g2: group 2 flags left at start-up (bits) */
    TM_ESM_UNEXPECTED_CH,   /**< esm_unexpected_ch: channels of unexpected errors (bits) */
    TM_EPC_FIRST_ADDR,      /**< epc_first_addr: address in the first used CAM entry */
    TM_LINES_DROPPED,       /**< lines_dropped: lines lost to a full TX buffer      */
    TM_SAMPLES_MISSED,      /**< samples_missed: windows the main loop missed       */
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
