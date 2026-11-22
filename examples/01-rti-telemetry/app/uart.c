/**
 * @file    uart.c
 * @brief   Polled SCI transmitter with a ring buffer, for the XDS110 COM port.
 *
 * SCI1 is configured in HALCoGen (SCI1 tab: asynchronous, internal clock,
 * 115200 baud, 8 data bits, 1 stop bit, no parity, no interrupts). This module
 * calls sciInit() and moves bytes with HALCoGen's polled functions. It never
 * waits: a byte goes out only when sciIsTxReady() says the SCI can take it.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#include "uart.h"
#include "app_config.h"
#include "HL_sci.h"

/** Ring buffer index mask; APP_TX_BUFFER_SIZE is a power of two. */
#define TX_MASK (APP_TX_BUFFER_SIZE - 1U)

/*------------------------------------------------------------------------------
 * Transmit ring buffer. Indices run freely and wrap; head - tail is the fill.
 *----------------------------------------------------------------------------*/

static uint8  s_tx[APP_TX_BUFFER_SIZE];
static uint32 s_txHead = 0U;   /* next byte to write */
static uint32 s_txTail = 0U;   /* next byte to send  */

/*------------------------------------------------------------------------------
 * Public functions
 *----------------------------------------------------------------------------*/

void Uart_Init(void)
{
    sciInit();

    s_txHead = 0U;
    s_txTail = 0U;
}

boolean Uart_Write(const uint8 *data, uint32 length)
{
    const uint32 free = APP_TX_BUFFER_SIZE - (s_txHead - s_txTail);
    boolean queued = FALSE;
    uint32 i;

    if (length <= free)
    {
        for (i = 0U; i < length; i++)
        {
            s_tx[(s_txHead + i) & TX_MASK] = data[i];
        }
        s_txHead += length;
        queued = TRUE;
    }

    return queued;
}

uint32 Uart_Pump(void)
{
    uint32 sent = 0U;

    /* sciSendByte() waits for TX ready; checking first means it never does. */
    while ((s_txHead != s_txTail) && (sciIsTxReady(APP_SCI) != 0U))
    {
        sciSendByte(APP_SCI, s_tx[s_txTail & TX_MASK]);
        s_txTail++;
        sent++;
    }

    return sent;
}

boolean Uart_ReadByte(uint8 *byte)
{
    boolean received = FALSE;

    /* Clears framing, overrun and parity errors; the byte, if any, is kept. */
    (void)sciRxError(APP_SCI);

    /* sciReceiveByte() waits for RX ready; checking first means it never does. */
    if (sciIsRxReady(APP_SCI) != 0U)
    {
        *byte = (uint8)sciReceiveByte(APP_SCI);
        received = TRUE;
    }

    return received;
}
