/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_BASE64_H
#define HOST_BASE64_H

#include <stddef.h>
#include <string.h>

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

/*
 * Base64 encode the UTF-16LE expansion of ISO-8859-1 text, as used by
 * PowerShell's -EncodedCommand. ISO-8859-1 code points map directly to
 * U+0000..U+00FF, so each input byte becomes the byte pair (c, 0x00).
 * Returns 0 when the output buffer is too small.
 */
static inline int host_base64_encode_utf16le(const char *text, char *out, size_t out_size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t len = strlen(text);
    size_t pos = 0;
    unsigned char group[3];
    int fill = 0;

    for (size_t i = 0; i < len * 2; i++) {
        group[fill++] = (i & 1) ? 0 : (unsigned char)text[i / 2];
        if (fill == 3) {
            if (pos + 4 >= out_size) {
                return 0;
            }
            out[pos++] = alphabet[group[0] >> 2];
            out[pos++] = alphabet[((group[0] & 0x03) << 4) | (group[1] >> 4)];
            out[pos++] = alphabet[((group[1] & 0x0f) << 2) | (group[2] >> 6)];
            out[pos++] = alphabet[group[2] & 0x3f];
            fill = 0;
        }
    }

    if (fill > 0) {
        if (pos + 4 >= out_size) {
            return 0;
        }
        group[fill] = 0;
        out[pos++] = alphabet[group[0] >> 2];
        out[pos++] = alphabet[((group[0] & 0x03) << 4) | (group[1] >> 4)];
        out[pos++] = (fill == 2) ? alphabet[(group[1] & 0x0f) << 2] : '=';
        out[pos++] = '=';
    }

    if (pos >= out_size) {
        return 0;
    }
    out[pos] = '\0';
    return 1;
}

#endif
