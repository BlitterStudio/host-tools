/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_POWERSHELL_H
#define HOST_POWERSHELL_H

#include <stddef.h>
#include "host_base64.h"
#include "host_common.h"

/*
 * Append text as a PowerShell single-quoted string literal, where the
 * only escape is doubling embedded quotes.
 */
static inline int host_append_ps_quoted(char *dest, size_t dest_size, const char *text)
{
    size_t pos = strlen(dest);

    if (pos + 1 >= dest_size) {
        return 0;
    }
    dest[pos++] = '\'';

    for (const char *p = text; *p; p++) {
        size_t need = (*p == '\'') ? 2 : 1;
        if (pos + need + 1 >= dest_size) {
            return 0;
        }
        dest[pos++] = *p;
        if (*p == '\'') {
            dest[pos++] = '\'';
        }
    }

    if (pos + 1 >= dest_size) {
        return 0;
    }
    dest[pos++] = '\'';
    dest[pos] = '\0';
    return 1;
}

/*
 * Append a complete "powershell -NoProfile -EncodedCommand <base64>"
 * command for the given script. The encoding carries arbitrary script
 * text (quotes, newlines, all of ISO-8859-1) safely through the
 * Windows command interpreter.
 */
static inline int host_append_ps_encoded_command(char *command, size_t command_size,
                                                 const char *script)
{
    static char encoded[HOST_MAX_COMMAND_LEN * 3];

    return host_base64_encode_utf16le(script, encoded, sizeof(encoded)) &&
           host_append_literal(command, command_size,
                               "powershell -NoProfile -EncodedCommand ") &&
           host_append_literal(command, command_size, encoded);
}

#endif
