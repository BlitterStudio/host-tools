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
 * host_append_ps_encoded_command. Everything the tools pass in is
 * ISO-8859-1: guest text, and the host paths from NativeDosOp, which
 * Amiberry keeps in ISO-8859-1 on every host. host_append_ps_quoted
 * converts it to UTF-8 as it quotes.
 */

static inline int host_ps_put(char *dest, size_t dest_size, size_t *pos, char c)
{
    if (*pos + 1 >= dest_size) {
        return 0;
    }
    dest[(*pos)++] = c;
    return 1;
}

/*
 * Append ISO-8859-1 text as a PowerShell single-quoted string literal
 * (in UTF-8), where the only escape is doubling embedded quotes. dest is
 * left unchanged on failure.
 */
static inline int host_append_ps_quoted(char *dest, size_t dest_size, const char *text)
{
    size_t start = strlen(dest);
    size_t pos = start;
    const unsigned char *p = (const unsigned char *)text;

    if (!host_ps_put(dest, dest_size, &pos, '\'')) {
        return 0;
    }

    for (; *p; p++) {
        int ok;

        if (*p >= 0x80) {
            ok = host_ps_put(dest, dest_size, &pos, (char)(0xC0 | (*p >> 6))) &&
                 host_ps_put(dest, dest_size, &pos, (char)(0x80 | (*p & 0x3F)));
        } else {
            ok = host_ps_put(dest, dest_size, &pos, (char)*p) &&
                 (*p != '\'' || host_ps_put(dest, dest_size, &pos, '\''));
        }
        if (!ok) {
            dest[start] = '\0';
            return 0;
        }
    }

    if (!host_ps_put(dest, dest_size, &pos, '\'')) {
        dest[start] = '\0';
        return 0;
    }
    dest[pos] = '\0';
    return 1;
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
