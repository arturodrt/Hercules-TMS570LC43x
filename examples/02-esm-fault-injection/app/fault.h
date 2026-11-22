/**
 * @file    fault.h
 * @brief   Injects two real faults into the TMS570LC43x and watches the ESM
 *          report them.
 *
 *  - Flash single-bit ECC error: one bit of one flash read is flipped on its way
 *    to the CPU. The CPU's SECDED logic corrects it, the Error Profiling
 *    Controller (EPC) records the address, and the ESM raises group 1
 *    channel 4. The program keeps running; nothing else happens.
 *
 *  - Lockstep compare error: the CPU Compare Module (CCM-R5F) is put in error
 *    forcing mode, which presents the two lockstep CPUs as different for one
 *    cycle. The ESM raises group 2 channel 2, interrupts the CPU with an FIQ
 *    and drives the ERROR pin low (the ERR LED lights). The application
 *    releases the pin APP_NERROR_HOLD_MS later.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef FAULT_H
#define FAULT_H

#include "HL_sys_common.h"

/** What the fault module has done and seen so far. */
typedef struct
{
    uint32  eccInjected;        /**< Flash single-bit errors injected. */
    uint32  eccDetected;        /**< EPC correctable error events (ESM 1.4) seen. */
    boolean eccReadOk;          /**< The last injected read returned the true data. */
    uint32  epcCamUsed;         /**< Addresses the EPC currently remembers (0-32). */
    uint32  ccmInjected;        /**< Lockstep compare errors forced. */
    uint32  ccmDetected;        /**< CPU compare errors (ESM 2.2) reported by FIQ. */
    uint32  ccmSelfTestFlags;   /**< CCM self-test flags (ESM 1.31) seen. */
    uint32  ccmDetectCycles;    /**< CPU cycles from forcing to the ESM handler. */
    uint32  unexpected;         /**< Group 2 errors this module did not inject. */
    boolean nErrorActive;       /**< The ESM drives the ERROR pin low. */
} FaultStatus;

/**
 * @brief  Prepares both injections: copies the flash ECC routine to RAM and
 *         opens an MPU window that lets it execute there, enables EPC error
 *         events, and enables the CPU event export to the EPC.
 *
 * @return TRUE if flash ECC injection is available. FALSE if the RAM routine
 *         does not fit its MPU window (see HL_sys_link.cmd); lockstep injection
 *         works either way.
 *
 * @note   Call before enabling FIQ and IRQ. Requires the PMU cycle counter.
 */
boolean Fault_Init(void);

/** @brief Injects one flash single-bit ECC error. Takes a few microseconds. */
void Fault_InjectFlashEcc(void);

/** @brief Forces one lockstep compare error. The FIQ follows immediately. */
void Fault_InjectLockstepError(void);

/**
 * @brief  Collects ESM group 1 flags, forgets injected EPC addresses, and
 *         releases the ERROR pin once APP_NERROR_HOLD_MS has passed.
 *         Call on every pass of the main loop.
 *
 * @param  nowMs  Current time in ms (Tick_Now()).
 */
void Fault_Poll(uint32 nowMs);

/** @brief Copies the current counters and states into @p status. */
void Fault_GetStatus(FaultStatus *status);

#endif /* FAULT_H */
