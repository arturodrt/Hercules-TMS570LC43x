/**
 * @file    telemetry.c
 * @brief   Telemetry as CSV text lines: one header line, then one line per sample.
 *
 * Numbers are formatted here rather than with printf: the formatter is a few
 * dozen lines, runs in bounded time, needs no heap, and pulls in none of the
 * run-time library's stdio.
 *
 * @author  Arturo de los Rios
 * @date    2026-11-21
 */

#include "telemetry.h"
#include "uart.h"

/** Longest line: 20 fields of at most 22 characters (sign, 20 digits, point),
 *  19 commas, CR LF: 461 bytes. */
#define LINE_MAX 464U

/** How each channel is named in the header and how many decimals it has. */
typedef struct
{
    const char *name;
    uint32 decimals;
} ChannelFormat;

static const ChannelFormat k_channels[TM_CHANNEL_COUNT] =
{
    { "t_ms",               0U },
    { "seq",                0U },
    { "btn_a",              0U },
    { "btn_b",              0U },
    { "nerror",             0U },
    { "ecc_injected",       0U },
    { "ecc_detected",       0U },
    { "ecc_read_ok",        0U },
    { "epc_cam_used",       0U },
    { "ccm_injected",       0U },
    { "ccm_detected",       0U },
    { "ccm_selftest_flags", 0U },
    { "ccm_detect_cycles",  0U },
    { "esm_unexpected",     0U },
    { "esm_boot_g1",        0U },
    { "esm_boot_g2",        0U },
    { "esm_unexpected_ch",  0U },
    { "epc_first_addr",     0U },
    { "lines_dropped",      0U },
    { "samples_missed",     0U },
};

/** A line being built. Writes past the end are ignored and flagged. */
typedef struct
{
    uint8  text[LINE_MAX];
    uint32 length;
    boolean overflow;
} Line;

static uint32 s_linesDropped = 0U;

/*------------------------------------------------------------------------------
 * Line building
 *----------------------------------------------------------------------------*/

static void Line_PutChar(Line *line, char c)
{
    if (line->length < LINE_MAX)
    {
        line->text[line->length] = (uint8)c;
        line->length++;
    }
    else
    {
        line->overflow = TRUE;
    }
}

static void Line_PutText(Line *line, const char *text)
{
    while (*text != '\0')
    {
        Line_PutChar(line, *text);
        text++;
    }
}

/**
 * @brief  Writes @p value as a decimal number with @p decimals digits after the
 *         point. The value is scaled: Line_PutFixed(line, -1234, 3) writes
 *         "-1.234", and Line_PutFixed(line, 5, 1) writes "0.5".
 */
static void Line_PutFixed(Line *line, sint64 value, uint32 decimals)
{
    char digits[24];
    uint32 count = 0U;
    uint64 magnitude;

    if (value < 0)
    {
        Line_PutChar(line, '-');
        /* Negate in unsigned arithmetic, which is defined even for INT64_MIN. */
        magnitude = 0U - (uint64)value;
    }
    else
    {
        magnitude = (uint64)value;
    }

    /* Least significant digit first; at least one digit before the point. */
    do
    {
        digits[count] = (char)('0' + (char)(magnitude % 10U));
        magnitude /= 10U;
        count++;
    } while (((magnitude != 0U) || (count <= decimals)) && (count < sizeof(digits)));

    while (count > 0U)
    {
        count--;
        Line_PutChar(line, digits[count]);
        if ((count == decimals) && (decimals != 0U))
        {
            Line_PutChar(line, '.');
        }
    }
}

static boolean Line_Send(const Line *line)
{
    return (line->overflow == FALSE) ? Uart_Write(line->text, line->length) : FALSE;
}

/*------------------------------------------------------------------------------
 * Public functions
 *----------------------------------------------------------------------------*/

boolean Telemetry_SendHeader(void)
{
    Line line;
    uint32 i;

    line.length = 0U;
    line.overflow = FALSE;

    for (i = 0U; i < (uint32)TM_CHANNEL_COUNT; i++)
    {
        if (i != 0U)
        {
            Line_PutChar(&line, ',');
        }
        Line_PutText(&line, k_channels[i].name);
    }
    Line_PutText(&line, "\r\n");

    return Line_Send(&line);
}

boolean Telemetry_SendSample(const sint64 values[TM_CHANNEL_COUNT])
{
    Line line;
    boolean sent;
    uint32 i;

    line.length = 0U;
    line.overflow = FALSE;

    for (i = 0U; i < (uint32)TM_CHANNEL_COUNT; i++)
    {
        if (i != 0U)
        {
            Line_PutChar(&line, ',');
        }
        Line_PutFixed(&line, values[i], k_channels[i].decimals);
    }
    Line_PutText(&line, "\r\n");

    sent = Line_Send(&line);
    if (sent == FALSE)
    {
        s_linesDropped++;
    }

    return sent;
}

uint32 Telemetry_LinesDropped(void)
{
    return s_linesDropped;
}
