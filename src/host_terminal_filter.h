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

struct host_terminal_filter
{
    int state;
    int utf8_remaining;
};

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

/*
 * Convert host ANSI CSI sequences to the Amiga console's single-byte CSI and
 * discard OSC sequences, which the Amiga console does not support. The state
 * is retained so escape sequences may span HostShell_Read() calls.
 *
 * The output can be at most input_len + 1 bytes when a pending ESC from the
 * previous call turns out to be literal.
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
                if (c == 0x1B) {
                    filter->state = HOST_TERMINAL_ESCAPE;
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
                    if (!host_terminal_filter_put(output, output_size, &output_len, 0x1B) ||
                        !host_terminal_filter_put(output, output_size, &output_len, c)) {
                        return -1;
                    }
                    filter->state = HOST_TERMINAL_TEXT;
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

    filter->state = HOST_TERMINAL_TEXT;
    filter->utf8_remaining = 0;
    return output_len;
}

#endif
