/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_POWERSHELL_H
#define HOST_POWERSHELL_H

#include <stddef.h>
#include "host_base64.h"
#include "host_common.h"

/* Script prefix that turns any PowerShell error into a non-zero exit code */
#define HOST_PS_STOP_ON_ERROR "$ErrorActionPreference='Stop';"

/*
 * PowerShell scripts are composed in UTF-8 and converted to UTF-16 by
 * host_append_ps_encoded_command. Guest text is ISO-8859-1 and native
 * host paths are UTF-8, so there are two quoting functions; mixing them
 * up garbles every non-ASCII character.
 */

static inline int host_ps_put(char *dest, size_t dest_size, size_t *pos, char c)
{
    if (*pos + 1 >= dest_size) {
        return 0;
    }
    dest[(*pos)++] = c;
    return 1;
}

/* Windows PowerShell also treats U+2018 to U+201B as single quotes */
static inline int host_ps_is_smart_quote(const unsigned char *p)
{
    return p[0] == 0xE2 && p[1] == 0x80 && p[2] >= 0x98 && p[2] <= 0x9B;
}

/*
 * Append text as a PowerShell single-quoted string literal, where the
 * only escape is doubling embedded quotes. latin1 selects the encoding
 * of text: ISO-8859-1 guest text (converted to UTF-8) or UTF-8.
 */
static inline int host_append_ps_quoted_text(char *dest, size_t dest_size, const char *text,
                                             int latin1)
{
    size_t start = strlen(dest);
    size_t pos = start;
    const unsigned char *p = (const unsigned char *)text;

    if (!host_ps_put(dest, dest_size, &pos, '\'')) {
        return 0;
    }

    while (*p) {
        size_t len = 1;
        int repeat = 1;

        if (latin1 && *p >= 0x80) {
            if (!host_ps_put(dest, dest_size, &pos, (char)(0xC0 | (*p >> 6))) ||
                !host_ps_put(dest, dest_size, &pos, (char)(0x80 | (*p & 0x3F)))) {
                dest[start] = '\0';
                return 0;
            }
            p++;
            continue;
        }
        if (*p == '\'') {
            repeat = 2;
        } else if (!latin1 && host_ps_is_smart_quote(p)) {
            len = 3;
            repeat = 2;
        }
        for (int r = 0; r < repeat; r++) {
            for (size_t i = 0; i < len; i++) {
                if (!host_ps_put(dest, dest_size, &pos, (char)p[i])) {
                    dest[start] = '\0';
                    return 0;
                }
            }
        }
        p += len;
    }

    if (!host_ps_put(dest, dest_size, &pos, '\'')) {
        dest[start] = '\0';
        return 0;
    }
    dest[pos] = '\0';
    return 1;
}

/* Guest text (ISO-8859-1) as a PowerShell string literal */
static inline int host_append_ps_quoted(char *dest, size_t dest_size, const char *text)
{
    return host_append_ps_quoted_text(dest, dest_size, text, 1);
}

/* Native host text (a path from NativeDosOp, UTF-8) as a PowerShell string literal */
static inline int host_append_ps_quoted_utf8(char *dest, size_t dest_size, const char *text)
{
    return host_append_ps_quoted_text(dest, dest_size, text, 0);
}

/*
 * Wrapper that makes a failing script print PowerShell's error message
 * (ISO-8859-1, like the other output) and exit 1. The pipe session
 * discards stderr, so without it the guest only sees a failure status.
 */
#define HOST_PS_TRY "try{"
#define HOST_PS_CATCH \
    "}catch{[Console]::OutputEncoding=[Text.Encoding]::GetEncoding(28591);" \
    "[Console]::Out.WriteLine($_.Exception.Message);exit 1}"

/*
 * Append a complete "powershell -NoProfile -EncodedCommand <base64>"
 * command for the given UTF-8 script. The encoding carries arbitrary
 * script text (quotes, newlines, any Unicode) safely through the
 * Windows command interpreter. The script runs inside HOST_PS_TRY and
 * HOST_PS_CATCH.
 */
static inline int host_append_ps_encoded_command(char *command, size_t command_size,
                                                 const char *script)
{
    static char wrapped[HOST_MAX_COMMAND_LEN * 2];
    static char encoded[HOST_MAX_COMMAND_LEN * 3];

    wrapped[0] = '\0';
    return host_append_literal(wrapped, sizeof(wrapped), HOST_PS_TRY) &&
           host_append_literal(wrapped, sizeof(wrapped), script) &&
           host_append_literal(wrapped, sizeof(wrapped), HOST_PS_CATCH) &&
           host_base64_encode_utf16le(wrapped, encoded, sizeof(encoded)) &&
           host_append_literal(command, command_size,
                               "powershell -NoProfile -EncodedCommand ") &&
           host_append_literal(command, command_size, encoded);
}

#endif
