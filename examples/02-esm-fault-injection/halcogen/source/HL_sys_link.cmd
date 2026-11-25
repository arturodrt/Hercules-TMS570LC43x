/*----------------------------------------------------------------------------*/
/* sys_link.cmd                                                               */
/*                                                                            */
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

/*                                                                            */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN (0) */
/* USER CODE END */


/*----------------------------------------------------------------------------*/
/* Linker Settings                                                            */

--retain="*(.intvecs)"

/* USER CODE BEGIN (1) */
/* USER CODE END */

/*----------------------------------------------------------------------------*/
/* Memory Map                                                                 */

MEMORY
{
/* USER CODE BEGIN (2) */
/* USER CODE END */
    VECTORS (X)  : origin=0x00000000 length=0x00000020 fill = 0xffffffff
    FLASH0  (RX) : origin=0x00000020 length=0x0003FFE0 fill = 0xffffffff
    FLASH1  (RX) : origin=0x00200000 length=0x00200000
    STACKS  (RW) : origin=0x08000000 length=0x00001500
    RAM     (RW) : origin=0x08001500 length=0x0007EB00

/* USER CODE BEGIN (3) */
    /* 02-esm-fault-injection: FLASH0 above is cut to the first 256 KB and,
     * with VECTORS, filled with 0xFFFFFFFF (re-apply both after regenerating
     * with HALCoGen). Every byte there is then programmed, so the flash
     * loader's Auto ECC Generation writes valid ECC for the holes between
     * sections and for the space past the image too, and a cache line fill or
     * prefetch there no longer raises ESM group 2 channel 3. The rest of
     * bank 0 is listed here unused, so nothing is placed in it by accident. */
    FLASH0B (RX) : origin=0x00040000 length=0x001C0000
/* USER CODE END */
}

/* USER CODE BEGIN (4) */
/* USER CODE END */


/*----------------------------------------------------------------------------*/
/* Section Configuration                                                      */

SECTIONS
{
/* USER CODE BEGIN (5) */
    /* 02-esm-fault-injection: the flash ECC injection routine must not run
     * from flash (TRM 7.7.2.2), so it is stored in flash and copied to RAM
     * by copy_in() before use. The run address is aligned to 256 bytes so
     * that one 256-byte MPU region can make exactly this code executable. */
    .ramfunc : load = FLASH0 | FLASH1, run = RAM, align(256),
               RUN_START(ramfuncRunStart), RUN_SIZE(ramfuncRunSize),
               table(ramfuncCopyTable)
    .ovly    : {} > FLASH0 | FLASH1
/* USER CODE END */
    .intvecs : {} > VECTORS
    .text   align(32) : {} > FLASH0 | FLASH1
    .const  align(32) : {} > FLASH0 | FLASH1
    .cinit  align(32) : {} > FLASH0 | FLASH1
    .pinit  align(32) : {} > FLASH0 | FLASH1
    .bss     : {} > RAM
    .data    : {} > RAM
    .sysmem  : {} > RAM
	

/* USER CODE BEGIN (6) */
/* USER CODE END */
}

/* USER CODE BEGIN (7) */
/* USER CODE END */


/*----------------------------------------------------------------------------*/
/* Misc                                                                       */

/* USER CODE BEGIN (8) */
/* USER CODE END */
/*----------------------------------------------------------------------------*/

