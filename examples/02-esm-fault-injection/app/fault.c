/**
 * @file    fault.c
 * @brief   Injects flash ECC and lockstep compare faults and tracks the ESM.
 *
 * Every register sequence here follows the TMS570LC43x Technical Reference
 * Manual (SPNU563A) and the TMS570LC4357 datasheet (SPNS195C); section numbers
 * are given next to each one.
 *
 * Flash single-bit ECC error (TRM 7.7.2.2, ECC Data Correction Diagnostic Test
 * Mode): with DIAGMODE 7, the flash controller XORs FEMU_DMSW/FEMU_DLSW into
 * the data of the next flash read before it reaches the CPU. Flipping one data
 * bit and leaving the stored ECC alone is exactly a single-bit memory fault:
 * the CPU's SECDED logic corrects it, so the read still returns the true data,
 * and reports the address to the EPC, which raises ESM group 1 channel 4.
 *
 * Two rules from the TRM shape the code:
 *  - "Branch to a non-Flash region to execute this sequence." Any flash access
 *    between the trigger and the read, an instruction fetch included, would use
 *    up the one corrupted read. The routine is linked into the .ramfunc
 *    section, copied to RAM by copy_in(), and runs with IRQ and FIQ masked.
 *  - "Make sure the address to be used for diagnostic is not already cached."
 *    The routine invalidates the data cache line before the read.
 *
 * HALCoGen's MPU setup makes all RAM non-executable (region 3), so Fault_Init()
 * opens MPU region 15 as a 256-byte, non-cacheable, read-only, executable
 * window over exactly the copied routine.
 *
 * Lockstep compare error (TRM 13.2.1.3, Error Forcing Mode): writing key 9h to
 * CCMKEYR1 makes the CCM-R5F see the two CPUs differ for one cycle, then it
 * returns to lockstep on its own. The ESM raises group 2 channel 2 (and, as a
 * second path, group 1 channel 31), drives the ERROR pin low, and enters
 * HALCoGen's esmHighInterrupt() as an FIQ, which calls esmGroup2Notification()
 * below.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#include <cpy_tbl.h>

#include "fault.h"
#include "app_config.h"
#include "tick.h"
#include "HL_esm.h"
#include "HL_epc.h"
#include "HL_reg_epc.h"
#include "HL_reg_flash.h"
#include "HL_reg_ccmr5.h"
#include "HL_sys_core.h"
#include "HL_sys_mpu.h"
#include "HL_sys_pmu.h"

/*------------------------------------------------------------------------------
 * ESM channels (datasheet SPNS195C, Table 6-45)
 *----------------------------------------------------------------------------*/

#define ESM_G1_EPC_CORRECTABLE      4U      /* EPC - Correctable Error       */
#define ESM_G1_CCM_SELFTEST         31U     /* CCM-R5F - self-test           */
#define ESM_G2_CCM_CPU_COMPARE      2U      /* CCM-R5F - CPU compare error   */

/** ESMEPSR.EPSF: 0 while the ERROR pin is low (TRM Table 16-12). */
#define ESM_EPSR_PIN_HIGH           1U

/** ESMEKR value that resets the ERROR pin to normal operation (TRM 16.2.2). */
#define ESM_EKR_ERROR_PIN_RESET     0x5U

/** Every channel of one status register; the flags are write-1-to-clear. */
#define ESM_ALL_CHANNELS            0xFFFFFFFFU

/*------------------------------------------------------------------------------
 * CCM-R5F key register 1 (TRM Table 13-9)
 *----------------------------------------------------------------------------*/

#define CCM_MKEY_MASK               0xFU
#define CCM_MKEY_LOCKSTEP           0x0U
#define CCM_MKEY_ERROR_FORCING      0x9U

/** Reads of CCMKEYR1 to wait for the return to lockstep (it takes one cycle). */
#define CCM_RETURN_POLLS            1000U

/*------------------------------------------------------------------------------
 * L2FMC diagnostic control (TRM 7.10.23)
 *----------------------------------------------------------------------------*/

#define FDIAG_KEY_ENABLE            (0x5U << 16U)   /* DIAG_EN_KEY = 5h           */
#define FDIAG_KEY_DISABLE           (0xAU << 16U)   /* DIAG_EN_KEY = Ah           */
#define FDIAG_MODE_ECC_DATA         0x7U            /* DIAGMODE 7                 */
#define FDIAG_BUF_PORT_A            (0x0U << 8U)    /* DIAG_BUF_SEL: port A       */
#define FDIAG_TRIG                  (1U << 24U)     /* arm for exactly one read   */

/**
 * Bit flipped in the lower 32 data bits: data bit 0, whose syndrome is CEh
 * (TRM Table 7-2), a single-bit error the CPU corrects.
 */
#define FLASH_FLIP_DATA_LSW         0x00000001U

/*------------------------------------------------------------------------------
 * EPC (TRM 12.4)
 *----------------------------------------------------------------------------*/

#define EPC_CAM_ENTRIES             32U
#define EPC_CAM_TAG_MASK            0xFU
#define EPC_CAM_TAG_AVAILABLE       0x5U
#define EPC_CAM_TAG_OCCUPIED        0xAU
#define EPC_CAM_ADDRESS_MASK        0xFFFFFFF8U     /* 64-bit aligned address */
#define EPC_NUMCAMAVAIL_MASK        0x3FU

/*------------------------------------------------------------------------------
 * The flash word the injection reads, and the RAM window for the routine
 *----------------------------------------------------------------------------*/

/** Cortex-R5F data cache line size. */
#define CACHE_LINE_BYTES            32U

/** Size of MPU region 15 over the RAM routine; HALCoGen's MPU_256_BYTES. */
#define RAMFUNC_WINDOW_BYTES        256U

#define ECC_TARGET_WORD0            0x5AFEC0DEU
#define ECC_TARGET_WORD1            0x0BADF00DU

/*
 * One cache line of flash that nothing else reads. The first 64-bit word is
 * the one whose read is corrupted; the rest of the line pads it so that no
 * other data shares the cache line.
 */
#pragma DATA_ALIGN(s_eccTarget, CACHE_LINE_BYTES)
static const uint32 s_eccTarget[CACHE_LINE_BYTES / 4U] =
{
    ECC_TARGET_WORD0, ECC_TARGET_WORD1, 0U, 0U, 0U, 0U, 0U, 0U
};

/* Linker symbols from HL_sys_link.cmd (USER CODE 5). */
extern COPY_TABLE ramfuncCopyTable;
extern uint32 ramfuncRunStart;
extern uint32 ramfuncRunSize;

/*------------------------------------------------------------------------------
 * State
 *----------------------------------------------------------------------------*/

static boolean s_flashReady = FALSE;
static uint32  s_eccInjected = 0U;
static uint32  s_eccDetected = 0U;
static boolean s_eccReadOk = FALSE;
static uint32  s_ccmSelfTestFlags = 0U;

/* ESM flags found set at start-up, left from before a warm reset. */
static uint32  s_bootGroup1 = 0U;
static uint32  s_bootGroup2 = 0U;

/* Written by the ESM FIQ, read by the main loop. */
static volatile uint32  s_ccmInjected = 0U;
static volatile uint32  s_ccmStartCycles = 0U;
static volatile uint32  s_ccmDetected = 0U;
static volatile uint32  s_ccmDetectCycles = 0U;
static volatile uint32  s_unexpected = 0U;
static volatile uint32  s_unexpectedChannels = 0U;  /* bit n: group 2 channel n seen */
static volatile uint32  s_errorMs = 0U;
static volatile boolean s_releasePending = FALSE;

/*------------------------------------------------------------------------------
 * The RAM routine
 *----------------------------------------------------------------------------*/

static uint32 Fault_FlashEccRead(const volatile uint32 *address);

#pragma CODE_SECTION(Fault_FlashEccRead, ".ramfunc")

/**
 * @brief  Reads @p address once with one data bit flipped by the flash
 *         controller. Runs from RAM; the caller masks IRQ and FIQ.
 *
 * Calls nothing, so no instruction is fetched from flash between the trigger
 * and the read. The sequence is TRM 7.7.2.2, steps 2 to 6 and 9.
 *
 * @return The word the CPU received, after SECDED correction.
 */
static uint32 Fault_FlashEccRead(const volatile uint32 *address)
{
    uint32 value;

    /* The read must reach the flash, not the data cache: invalidate the line
     * (DCIMVAC). The flash line is never written, so nothing is lost. */
    __MCR(15U, 0U, (uint32)address, 7U, 6U, 1U);
    __asm(" DSB");

    /* Key and mode first, then the XOR values, then the trigger. */
    flashWREG->FDIAGCTRL = FDIAG_KEY_ENABLE | FDIAG_MODE_ECC_DATA;
    flashWREG->FEMU_DMSW = 0U;
    flashWREG->FEMU_DLSW = FLASH_FLIP_DATA_LSW;
    flashWREG->FEMU_ECC  = 0U;
    flashWREG->FDIAGCTRL = FDIAG_KEY_ENABLE | FDIAG_BUF_PORT_A | FDIAG_MODE_ECC_DATA
                         | FDIAG_TRIG;

    /* The one corrupted read. */
    value = *address;
    __asm(" DSB");

    /* Leave diagnostic mode. */
    flashWREG->FDIAGCTRL = FDIAG_KEY_DISABLE;
    __asm(" DSB");

    return value;
}

/*------------------------------------------------------------------------------
 * Local functions
 *----------------------------------------------------------------------------*/

/** Programs MPU region 15 over the RAM routine with @p permission. */
static void Fault_SetRamWindow(uint32 base, uint32 permission)
{
    __asm(" DSB");
    _mpuSetRegion_(mpuREGION15);
    _mpuSetRegionBaseAddress_(base);
    _mpuSetRegionTypeAndPermission_((uint32)MPU_NORMAL_OINC_NONSHARED, permission);
    _mpuSetRegionSizeRegister_(mpuREGION_ENABLE | (uint32)MPU_256_BYTES);
    __asm(" DSB");
    __asm(" ISB");
}

/**
 * Marks every EPC CAM entry that holds an address in the cache line at
 * @p line as available again, so the next error there is reported. Other
 * entries are written back with their own tag, which leaves them unchanged:
 * in functional mode the CPU can free an entry but not occupy one (TRM 12.2.2.1).
 */
static void Fault_ForgetLine(uint32 line)
{
    uint32 entry;

    for (entry = 0U; entry < EPC_CAM_ENTRIES; entry++)
    {
        const uint32 reg     = entry / 4U;
        const uint32 shift   = (entry % 4U) * 8U;
        const uint32 tags    = epcREG1->CAM_INDEX[reg];
        const uint32 address = epcREG1->CAM_CONTENT[entry] & EPC_CAM_ADDRESS_MASK;

        if ((((tags >> shift) & EPC_CAM_TAG_MASK) == EPC_CAM_TAG_OCCUPIED) &&
            (address >= line) && (address < (line + CACHE_LINE_BYTES)))
        {
            epcREG1->CAM_INDEX[reg] = (tags & ~(EPC_CAM_TAG_MASK << shift))
                                    | (EPC_CAM_TAG_AVAILABLE << shift);
        }
    }
}

/** @return The address in the first occupied EPC CAM entry, or 0 if none is. */
static uint32 Fault_FirstCamAddress(void)
{
    uint32 entry;
    uint32 address = 0U;

    for (entry = 0U; (entry < EPC_CAM_ENTRIES) && (address == 0U); entry++)
    {
        const uint32 tags = epcREG1->CAM_INDEX[entry / 4U];

        if (((tags >> ((entry % 4U) * 8U)) & EPC_CAM_TAG_MASK) == EPC_CAM_TAG_OCCUPIED)
        {
            address = epcREG1->CAM_CONTENT[entry] & EPC_CAM_ADDRESS_MASK;
        }
    }

    return address;
}

/** Marks every occupied EPC CAM entry as available again (see Fault_ForgetLine). */
static void Fault_ForgetAll(void)
{
    uint32 entry;

    for (entry = 0U; entry < EPC_CAM_ENTRIES; entry++)
    {
        const uint32 reg   = entry / 4U;
        const uint32 shift = (entry % 4U) * 8U;
        const uint32 tags  = epcREG1->CAM_INDEX[reg];

        if (((tags >> shift) & EPC_CAM_TAG_MASK) == EPC_CAM_TAG_OCCUPIED)
        {
            epcREG1->CAM_INDEX[reg] = (tags & ~(EPC_CAM_TAG_MASK << shift))
                                    | (EPC_CAM_TAG_AVAILABLE << shift);
        }
    }
}

/**
 * Takes over the ESM and the EPC as a warm reset left them.
 *
 * HALCoGen's start-up runs esmInit() after a power-on, external or debug reset
 * only (HL_sys_startup.c); after a watchdog, CPU or software reset the ESM's
 * flags and ERROR pin and the EPC's CAM are as the previous run left them.
 * Flags set before main() would reach the FIQ as soon as it is enabled and be
 * counted as unexpected, which keeps the ERROR pin low for good, and a pending
 * EPC flag would be counted as a detection nobody injected. So the flags are
 * recorded for the telemetry (esm_boot_g1, esm_boot_g2), then cleared, the CAM
 * is emptied and the ERROR pin is released. Group 3 is left alone: start-up
 * already reports it (esmGroup3Notification).
 *
 * Must run before FIQ is enabled.
 */
static void Fault_ClearLeftovers(void)
{
    s_bootGroup1 = esmREG->SR1[0U];
    s_bootGroup2 = esmREG->SR1[1U] | esmREG->SSR2;

    /* Groups 1 and 2, channels 0-31, 32-63 and 64-95, then the group 2
     * shadow register (TRM 16.4). */
    esmREG->SR1[0U] = ESM_ALL_CHANNELS;
    esmREG->SR4[0U] = ESM_ALL_CHANNELS;
    esmREG->SR7[0U] = ESM_ALL_CHANNELS;
    esmREG->SR1[1U] = ESM_ALL_CHANNELS;
    esmREG->SR4[1U] = ESM_ALL_CHANNELS;
    esmREG->SR7[1U] = ESM_ALL_CHANNELS;
    esmREG->SSR2    = ESM_ALL_CHANNELS;

    Fault_ForgetAll();

    if ((esmREG->EPSR & ESM_EPSR_PIN_HIGH) == 0U)
    {
        esmREG->EKR = ESM_EKR_ERROR_PIN_RESET;

        /* The pin is released only once its low time has run out; if it had
         * not yet, Fault_Poll() asks again APP_NERROR_HOLD_MS after start. */
        s_errorMs = 0U;
        s_releasePending = TRUE;
    }
}

/*------------------------------------------------------------------------------
 * ESM group 2 notification (called from HALCoGen's esmHighInterrupt FIQ)
 *----------------------------------------------------------------------------*/

/**
 * @brief  Replaces HALCoGen's weak esmGroup2Notification(). esmHighInterrupt()
 *         has already cleared the channel's flag in ESMSR2.
 */
void esmGroup2Notification(esmBASE_t *esm, uint32 channel)
{
    const uint32 now = _pmuGetCycleCount_();

    (void)esm;

    if ((channel == ESM_G2_CCM_CPU_COMPARE) && (s_ccmDetected < s_ccmInjected))
    {
        s_ccmDetectCycles = now - s_ccmStartCycles;
        s_ccmDetected++;
        s_errorMs = Tick_Now();
        s_releasePending = TRUE;
    }
    else
    {
        /* A group 2 error this module did not cause. It is a real fault: count
         * it, note its channel, and never release the ERROR pin from here on. */
        s_unexpected++;
        if (channel < 32U)
        {
            s_unexpectedChannels |= 1U << channel;
        }
    }
}

/*------------------------------------------------------------------------------
 * Public functions
 *----------------------------------------------------------------------------*/

boolean Fault_Init(void)
{
    const uint32 runStart = (uint32)&ramfuncRunStart;
    const uint32 runSize  = (uint32)&ramfuncRunSize;

    Fault_ClearLeftovers();

    /* The CPU reports ECC errors on its AXI bus over its event bus; HALCoGen's
     * start-up enables the export on power-on reset only. */
    _coreEnableEventBusExport_();

    /* Let the EPC raise ESM group 1 channel 4 for new correctable addresses. */
    epcEnableSERREvent();

    s_flashReady = FALSE;
    if (((runStart % RAMFUNC_WINDOW_BYTES) == 0U) &&
        (runSize != 0U) && (runSize <= RAMFUNC_WINDOW_BYTES))
    {
        /* Non-cacheable and writable first, so copy_in() writes straight to
         * RAM; then read-only and executable. */
        Fault_SetRamWindow(runStart, (uint32)MPU_PRIV_RW_USER_RW_NOEXEC);
        copy_in(&ramfuncCopyTable);
        Fault_SetRamWindow(runStart, (uint32)MPU_PRIV_RO_USER_RO_EXEC);
        s_flashReady = TRUE;
    }

    return s_flashReady;
}

void Fault_InjectFlashEcc(void)
{
    if (s_flashReady == TRUE)
    {
        uint32 state;
        uint32 word0;
        uint32 word1;

        state = _disable_interrupts();
        word0 = Fault_FlashEccRead(&s_eccTarget[0U]);
        _restore_interrupts(state);

        /* The second half of the same 64-bit word came in with the same line
         * fill and is now in the cache. One of the two halves held the flipped
         * bit; both must read back true. */
        word1 = *(const volatile uint32 *)&s_eccTarget[1U];

        s_eccReadOk = ((word0 == ECC_TARGET_WORD0) && (word1 == ECC_TARGET_WORD1)) ? TRUE : FALSE;
        s_eccInjected++;
    }
}

void Fault_InjectLockstepError(void)
{
    uint32 polls = 0U;

    s_ccmInjected++;
    s_ccmStartCycles = _pmuGetCycleCount_();

    ccmr5REG->CCMKEYR1 = CCM_MKEY_ERROR_FORCING;

    while (((ccmr5REG->CCMKEYR1 & CCM_MKEY_MASK) != CCM_MKEY_LOCKSTEP) &&
           (polls < CCM_RETURN_POLLS))
    {
        polls++;
    }
}

void Fault_Poll(uint32 nowMs)
{
    const uint32 group1 = esmREG->SR1[0U];

    if ((group1 & (1U << ESM_G1_EPC_CORRECTABLE)) != 0U)
    {
        /* Write 1 to clear. */
        esmREG->SR1[0U] = 1U << ESM_G1_EPC_CORRECTABLE;
        s_eccDetected++;

        if (APP_EPC_FORGET_ADDRESS == TRUE)
        {
            Fault_ForgetLine((uint32)&s_eccTarget[0U]);
        }
    }

    if ((group1 & (1U << ESM_G1_CCM_SELFTEST)) != 0U)
    {
        esmREG->SR1[0U] = 1U << ESM_G1_CCM_SELFTEST;
        s_ccmSelfTestFlags++;
    }

    if ((s_releasePending == TRUE) && (s_unexpected == 0U) &&
        ((nowMs - s_errorMs) >= APP_NERROR_HOLD_MS))
    {
        s_releasePending = FALSE;

        /* The group 2 flag itself was cleared by the FIQ; clear its shadow
         * copy too, then ask the ESM to release the pin (TRM 16.2.2). */
        esmREG->SSR2 = 1U << ESM_G2_CCM_CPU_COMPARE;
        esmTriggerErrorPinReset();
    }
}

void Fault_GetStatus(FaultStatus *status)
{
    status->eccInjected      = s_eccInjected;
    status->eccDetected      = s_eccDetected;
    status->eccReadOk        = s_eccReadOk;
    status->epcCamUsed       = EPC_CAM_ENTRIES - (epcREG1->CAMAVAILSTAT & EPC_NUMCAMAVAIL_MASK);
    status->ccmInjected      = s_ccmInjected;
    status->ccmDetected      = s_ccmDetected;
    status->ccmSelfTestFlags = s_ccmSelfTestFlags;
    status->ccmDetectCycles  = s_ccmDetectCycles;
    status->unexpected       = s_unexpected;
    status->bootGroup1       = s_bootGroup1;
    status->bootGroup2       = s_bootGroup2;
    status->unexpectedChannels = s_unexpectedChannels;
    status->epcFirstAddress  = Fault_FirstCamAddress();
    status->nErrorActive     = ((esmREG->EPSR & ESM_EPSR_PIN_HIGH) == 0U) ? TRUE : FALSE;
}
