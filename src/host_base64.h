/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_BASE64_H
#define HOST_BASE64_H

#include <stddef.h>

struct host_base64_state {
    unsigned long acc;
    int nbits;
    int padded;
};

static inline void host_base64_init(struct host_base64_state *st)
{
    st->acc = 0;
    st->nbits = 0;
    st->padded = 0;
}

static inline int host_base64_value(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

/*
 * Decode a chunk of base64 input, tolerating embedded whitespace and
 * carriage returns. State carries partial groups across chunks. out
 * must hold at least (len * 3) / 4 + 3 bytes. Returns the number of
 * decoded bytes, or -1 on invalid input (including data after the
 * '=' padding).
 */
static inline long host_base64_feed(struct host_base64_state *st,
                                    const char *in, long len,
                                    unsigned char *out)
{
    long n = 0;

    for (long i = 0; i < len; i++) {
        char c = in[i];
        int v;

        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            continue;
        }
        if (c == '=') {
            st->padded = 1;
            continue;
        }
        v = host_base64_value(c);
        if (v < 0 || st->padded) {
            return -1;
        }
        st->acc = (st->acc << 6) | (unsigned long)v;
        st->nbits += 6;
        if (st->nbits >= 8) {
            st->nbits -= 8;
            out[n++] = (unsigned char)((st->acc >> st->nbits) & 0xff);
        }
    }

    return n;
}

#endif
