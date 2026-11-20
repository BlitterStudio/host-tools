/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_TERMINAL_FILTER_H
#define HOST_TERMINAL_FILTER_H

#define HOST_TERMINAL_TEXT 0
#define HOST_TERMINAL_ESCAPE 1
#define HOST_TERMINAL_OSC 2
#define HOST_TERMINAL_OSC_ESCAPE 3

/*
 * Host terminal output is UTF-8, the Amiga console is ISO-8859-1. The
 * filter decodes UTF-8 (keeping a partial character between reads),
 * converts ANSI CSI sequences to the Amiga console's single-byte CSI and
 * discards OSC sequences, which the Amiga console does not support.
 */
struct host_terminal_filter
{
    int state;
    /* continuation bytes still missing from the UTF-8 character being read */
    int utf8_remaining;
    /* its sequence length and the code point read so far */
    int utf8_length;
    unsigned long code_point;
};

static inline void host_terminal_filter_init(struct host_terminal_filter *filter)
{
    filter->state = HOST_TERMINAL_TEXT;
    filter->utf8_remaining = 0;
    filter->utf8_length = 0;
    filter->code_point = 0;
}

static inline int host_terminal_filter_utf8_continuations(unsigned char value)
{
    if (value >= 0xC2 && value <= 0xDF) {
        return 1;
    }
    if (value >= 0xE0 && value <= 0xEF) {
        return 2;
    }
    if (value >= 0xF0 && value <= 0xF4) {
        return 3;
    }
    return 0;
}

static inline int host_terminal_filter_put(unsigned char *output, int output_size,
                                           int *output_len, unsigned char value)
{
    if (*output_len >= output_size) {
        return 0;
    }

    output[(*output_len)++] = value;
    return 1;
}

static inline int host_terminal_filter_put_text(unsigned char *output, int output_size,
                                                int *output_len, const char *text)
{
    for (; *text; text++) {
        if (!host_terminal_filter_put(output, output_size, output_len, (unsigned char)*text)) {
            return 0;
        }
    }
    return 1;
}

/* ASCII stand-ins for common characters that ISO-8859-1 lacks; others become '?' */
static inline const char *host_terminal_filter_ascii_for(unsigned long code_point)
{
    switch (code_point) {
        case 0x2018:
        case 0x2019:
            return "'";
        case 0x201C:
        case 0x201D:
            return "\"";
        case 0x2013:
        case 0x2014:
        case 0x2212:
            return "-";
        case 0x2022:
            return "*";
        case 0x2026:
            return "...";
        default:
            return "?";
    }
}

/*
 * Write a finished UTF-8 character as ISO-8859-1. Malformed sequences
 * (overlong, surrogates, above U+10FFFF) and characters with no
 * ISO-8859-1 form become ASCII stand-ins or '?'; C1 controls
 * (U+0080 to U+009F) are dropped because the Amiga console would act
 * on them.
 */
static inline int host_terminal_filter_put_code_point(unsigned char *output, int output_size,
                                                      int *output_len, int length,
                                                      unsigned long code_point)
{
    if ((length == 3 && code_point < 0x800) || (length == 4 && code_point < 0x10000) ||
        (code_point >= 0xD800 && code_point <= 0xDFFF) || code_point > 0x10FFFF) {
        return host_terminal_filter_put(output, output_size, output_len, '?');
    }
    if (code_point >= 0x80 && code_point <= 0x9F) {
        return 1;
    }
    if (code_point <= 0xFF) {
        return host_terminal_filter_put(output, output_size, output_len,
                                        (unsigned char)code_point);
    }
    return host_terminal_filter_put_text(output, output_size, output_len,
                                         host_terminal_filter_ascii_for(code_point));
}

/*
 * Convert host terminal output to what the Amiga console takes. The
 * state is retained so escape sequences and UTF-8 characters may span
 * HostShell_Read() calls. Bytes that are not valid UTF-8 pass through
 * unchanged; a character cut short by the next byte becomes '?'.
 *
 * The output can be at most input_len + 1 bytes: decoding never makes
 * text longer, and a pending ESC from the previous call may turn out to
 * be literal.
 */
static inline int host_terminal_filter_process(struct host_terminal_filter *filter,
                                               const unsigned char *input, int input_len,
                                               unsigned char *output, int output_size)
{
    int output_len = 0;

    if (filter == NULL || input == NULL || input_len < 0 ||
        output == NULL || output_size < 0) {
        return -1;
    }

    for (int i = 0; i < input_len; i++) {
        unsigned char c = input[i];

        switch (filter->state) {
            case HOST_TERMINAL_TEXT:
                if (filter->utf8_remaining > 0) {
                    if (c >= 0x80 && c <= 0xBF) {
                        filter->code_point = (filter->code_point << 6) | (c & 0x3F);
                        if (--filter->utf8_remaining == 0 &&
                            !host_terminal_filter_put_code_point(output, output_size, &output_len,
                                                                 filter->utf8_length,
                                                                 filter->code_point)) {
                            return -1;
                        }
                        break;
                    }
                    /* cut short: the character is lost, c starts over */
                    filter->utf8_remaining = 0;
                    if (!host_terminal_filter_put(output, output_size, &output_len, '?')) {
                        return -1;
                    }
                }
                if (c == 0x1B) {
                    filter->state = HOST_TERMINAL_ESCAPE;
                } else if (c == 0x9D) {
                    filter->state = HOST_TERMINAL_OSC;
                } else if (host_terminal_filter_utf8_continuations(c) > 0) {
                    filter->utf8_remaining = host_terminal_filter_utf8_continuations(c);
                    filter->utf8_length = filter->utf8_remaining + 1;
                    filter->code_point = c & (0xFF >> (filter->utf8_length + 1));
                } else if (!host_terminal_filter_put(output, output_size, &output_len, c)) {
                    return -1;
                }
                break;

            case HOST_TERMINAL_ESCAPE:
                if (c == '[') {
                    if (!host_terminal_filter_put(output, output_size, &output_len, 0x9B)) {
                        return -1;
                    }
                    filter->state = HOST_TERMINAL_TEXT;
                } else if (c == ']') {
                    filter->state = HOST_TERMINAL_OSC;
                    filter->utf8_remaining = 0;
                } else {
                    /* a literal ESC; c is read again as ordinary text */
                    if (!host_terminal_filter_put(output, output_size, &output_len, 0x1B)) {
                        return -1;
                    }
                    filter->state = HOST_TERMINAL_TEXT;
                    i--;
                }
                break;

            case HOST_TERMINAL_OSC:
                if (filter->utf8_remaining > 0 && c >= 0x80 && c <= 0xBF) {
                    filter->utf8_remaining--;
                } else if (c == 0x07 || c == 0x9C) {
                    filter->state = HOST_TERMINAL_TEXT;
                    filter->utf8_remaining = 0;
                } else if (c == 0x1B) {
                    filter->state = HOST_TERMINAL_OSC_ESCAPE;
                    filter->utf8_remaining = 0;
                } else {
                    filter->utf8_remaining = host_terminal_filter_utf8_continuations(c);
                }
                break;

            case HOST_TERMINAL_OSC_ESCAPE:
                if (c == '\\' || c == 0x07 || c == 0x9C) {
                    filter->state = HOST_TERMINAL_TEXT;
                    filter->utf8_remaining = 0;
                } else if (c != 0x1B) {
                    filter->state = HOST_TERMINAL_OSC;
                    filter->utf8_remaining = host_terminal_filter_utf8_continuations(c);
                }
                break;

            default:
                return -1;
        }
    }

    return output_len;
}

static inline int host_terminal_filter_finish(struct host_terminal_filter *filter,
                                              unsigned char *output, int output_size)
{
    int output_len = 0;

    if (filter == NULL || output == NULL || output_size < 0) {
        return -1;
    }

    if (filter->state == HOST_TERMINAL_ESCAPE &&
        !host_terminal_filter_put(output, output_size, &output_len, 0x1B)) {
        return -1;
    }
    if (filter->state == HOST_TERMINAL_TEXT && filter->utf8_remaining > 0 &&
        !host_terminal_filter_put(output, output_size, &output_len, '?')) {
        return -1;
    }

    host_terminal_filter_init(filter);
    return output_len;
}

/*
 * Convert typed Amiga input for the host: ISO-8859-1 becomes UTF-8, the
 * Amiga console's CSI (0x9B) becomes ESC [ and Backspace becomes DEL.
 * The output can be twice the input length. Returns the output length,
 * or -1 when output_size is too small.
 */
static inline int host_terminal_encode_input(const unsigned char *input, int input_len,
                                             unsigned char *output, int output_size)
{
    int output_len = 0;

    if (input == NULL || input_len < 0 || output == NULL || output_size < 0) {
        return -1;
    }

    for (int i = 0; i < input_len; i++) {
        unsigned char c = input[i];
        int ok;

        if (c == 0x9B) {
            ok = host_terminal_filter_put(output, output_size, &output_len, 0x1B) &&
                 host_terminal_filter_put(output, output_size, &output_len, '[');
        } else if (c == 0x08) {
            ok = host_terminal_filter_put(output, output_size, &output_len, 0x7F);
        } else if (c < 0x80) {
            ok = host_terminal_filter_put(output, output_size, &output_len, c);
        } else {
            ok = host_terminal_filter_put(output, output_size, &output_len,
                                          (unsigned char)(0xC0 | (c >> 6))) &&
                 host_terminal_filter_put(output, output_size, &output_len,
                                          (unsigned char)(0x80 | (c & 0x3F)));
        }
        if (!ok) {
            return -1;
        }
    }

    return output_len;
}

#endif
