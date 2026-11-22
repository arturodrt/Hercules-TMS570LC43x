/**
 * @file    uart.h
 * @brief   Polled SCI transmitter with a ring buffer, for the XDS110 COM port.
 *
 * The main loop queues whole lines with Uart_Write() and calls Uart_Pump() on
 * every pass to move bytes from the ring buffer to the SCI. Nothing here runs
 * in an interrupt, so the ring buffer needs no locking.
 *
 * Format, set in HALCoGen's SCI1 tab: 115200 baud, 8 data bits, no parity,
 * 1 stop bit (8N1).
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#ifndef UART_H
#define UART_H

#include "HL_sys_common.h"

/** @brief Initializes the SCI with HALCoGen's sciInit(). */
void Uart_Init(void);

/**
 * @brief  Queues @p length bytes, all of them or none.
 *
 * @return TRUE if the bytes were queued, FALSE if the ring buffer had no room.
 */
boolean Uart_Write(const uint8 *data, uint32 length);

/**
 * @brief  Moves queued bytes into the SCI while it can take them. Never waits.
 *
 * @return Number of bytes handed to the SCI.
 */
uint32 Uart_Pump(void);

/**
 * @brief  Reads one received byte, if there is one. Clears receive errors.
 *
 * @return TRUE if a byte was stored in @p byte.
 */
boolean Uart_ReadByte(uint8 *byte);

#endif /* UART_H */
