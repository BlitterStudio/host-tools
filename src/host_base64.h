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
 * Decode the next UTF-8 sequence of text into a code point. Returns the
 * sequence length, or 0 for malformed input (stray continuation byte,
 * truncated, overlong or surrogate sequence, or a value above U+10FFFF).
 */
static inline int host_utf8_decode(const unsigned char *text, unsigned long *code_point)
{
    unsigned long cp;
    int extra;

    if (text[0] < 0x80) {
        *code_point = text[0];
        return 1;
    }
    if (text[0] >= 0xC2 && text[0] <= 0xDF) {
        cp = text[0] & 0x1F;
        extra = 1;
    } else if (text[0] >= 0xE0 && text[0] <= 0xEF) {
        cp = text[0] & 0x0F;
        extra = 2;
    } else if (text[0] >= 0xF0 && text[0] <= 0xF4) {
        cp = text[0] & 0x07;
        extra = 3;
    } else {
        return 0;
    }

    for (int i = 1; i <= extra; i++) {
        if ((text[i] & 0xC0) != 0x80) {
            return 0;
        }
        cp = (cp << 6) | (text[i] & 0x3F);
    }

    if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
        (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
        return 0;
    }

    *code_point = cp;
    return extra + 1;
}

struct host_base64_encoder {
    char *out;
    size_t out_size;
    size_t pos;
    unsigned char group[3];
    int fill;
};

static inline int host_base64_put_byte(struct host_base64_encoder *enc, unsigned char value)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    enc->group[enc->fill++] = value;
    if (enc->fill == 3) {
        if (enc->pos + 4 >= enc->out_size) {
            return 0;
        }
        enc->out[enc->pos++] = alphabet[enc->group[0] >> 2];
        enc->out[enc->pos++] = alphabet[((enc->group[0] & 0x03) << 4) | (enc->group[1] >> 4)];
        enc->out[enc->pos++] = alphabet[((enc->group[1] & 0x0f) << 2) | (enc->group[2] >> 6)];
        enc->out[enc->pos++] = alphabet[enc->group[2] & 0x3f];
        enc->fill = 0;
    }
    return 1;
}

static inline int host_base64_put_utf16(struct host_base64_encoder *enc, unsigned long unit)
{
    return host_base64_put_byte(enc, (unsigned char)(unit & 0xff)) &&
           host_base64_put_byte(enc, (unsigned char)(unit >> 8));
}

/*
 * Base64 encode the UTF-16LE form of UTF-8 text, as used by PowerShell's
 * -EncodedCommand. Returns 0 when the text is not valid UTF-8 or the
 * output buffer is too small.
 */
static inline int host_base64_encode_utf16le(const char *text, char *out, size_t out_size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    struct host_base64_encoder enc;
    const unsigned char *p = (const unsigned char *)text;

    enc.out = out;
    enc.out_size = out_size;
    enc.pos = 0;
    enc.fill = 0;

    while (*p) {
        unsigned long cp;
        int len = host_utf8_decode(p, &cp);

        if (len == 0) {
            return 0;
        }
        p += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            if (!host_base64_put_utf16(&enc, 0xD800 + (cp >> 10)) ||
                !host_base64_put_utf16(&enc, 0xDC00 + (cp & 0x3FF))) {
                return 0;
            }
        } else if (!host_base64_put_utf16(&enc, cp)) {
            return 0;
        }
    }

    if (enc.fill > 0) {
        int fill = enc.fill;

        if (enc.pos + 4 >= out_size) {
            return 0;
        }
        enc.group[fill] = 0;
        out[enc.pos++] = alphabet[enc.group[0] >> 2];
        out[enc.pos++] = alphabet[((enc.group[0] & 0x03) << 4) | (enc.group[1] >> 4)];
        out[enc.pos++] = (fill == 2) ? alphabet[(enc.group[1] & 0x0f) << 2] : '=';
        out[enc.pos++] = '=';
    }

    if (enc.pos >= out_size) {
        return 0;
    }
    out[enc.pos] = '\0';
    return 1;
}

#endif
