/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CLIP_COMMAND_H
#define HOST_CLIP_COMMAND_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "host_common.h"
#include "host_powershell.h"

#define HOST_CLIP_INPUT_OK 0
#define HOST_CLIP_INPUT_TOO_LONG 1
#define HOST_CLIP_INPUT_READ_ERROR 2
#define HOST_CLIP_INPUT_HAS_NUL 3

/*
 * Read the clipboard text from a stream. The text is handed to the host
 * as a C string, so a NUL byte would silently cut it short, and a read
 * error would look like end of input; both are reported instead of
 * being copied. The result is always NUL terminated.
 */
static inline int host_clip_read_text(FILE *in, char *text, size_t text_size)
{
    size_t total;

    if (in == NULL || text == NULL || text_size == 0) {
        return HOST_CLIP_INPUT_READ_ERROR;
    }

    total = fread(text, 1, text_size - 1, in);
    text[total] = '\0';
    if (ferror(in)) {
        return HOST_CLIP_INPUT_READ_ERROR;
    }
    if (memchr(text, '\0', total) != NULL) {
        return HOST_CLIP_INPUT_HAS_NUL;
    }
    if (total == text_size - 1) {
        int c = getc(in);

        if (c != EOF) {
            return HOST_CLIP_INPUT_TOO_LONG;
        }
        if (ferror(in)) {
            return HOST_CLIP_INPUT_READ_ERROR;
        }
    }
    return HOST_CLIP_INPUT_OK;
}

#define HOST_CLIP_PASTE_COMMAND \
    "t=$(mktemp) || exit 1; c=\"${t}.converted\"; " \
    "trap 'rm -f \"$t\" \"$c\"' 0 1 2 15; " \
    "if command -v pbpaste >/dev/null 2>&1; then pbpaste >\"$t\"; " \
    "elif command -v wl-paste >/dev/null 2>&1; then wl-paste -n >\"$t\"; " \
    "elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard -o >\"$t\"; " \
    "elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --output >\"$t\"; " \
    "else printf 'No host clipboard backend found\\n' >&2; exit 127; fi || exit $?; " \
    "if command -v iconv >/dev/null 2>&1 && " \
    "iconv -c -f UTF-8 -t ISO-8859-1//TRANSLIT <\"$t\" >\"$c\" 2>/dev/null; " \
    "then cat \"$c\"; else cat \"$t\"; fi"

/*
 * Windows clipboard access goes through PowerShell. ISO-8859-1 text
 * converts to UTF-16 in the encoded command itself, and paste output
 * is written with an ISO-8859-1 console encoding, so no iconv is
 * involved on Windows.
 */
#define HOST_CLIP_PASTE_PS_SCRIPT \
    "[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(28591);" \
    "$t=Get-Clipboard -Raw;if($t){[Console]::Out.Write($t)}"

static inline int host_append_clip_paste_command_windows(char *command, size_t command_size)
{
    return host_append_ps_encoded_command(command, command_size, HOST_CLIP_PASTE_PS_SCRIPT);
}

static inline int host_append_clip_copy_command_windows(char *command, size_t command_size,
                                                        const char *text)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script), "Set-Clipboard -Value ") &&
           host_append_ps_quoted(script, sizeof(script), text) &&
           host_append_ps_encoded_command(command, command_size, script);
}

static inline int host_append_clip_copy_command(char *command, size_t command_size,
                                                const char *text)
{
    return host_append_literal(command, command_size, "printf %s ") &&
           host_append_shell_arg(command, command_size, text, 0) &&
           host_append_literal(command, command_size,
                               " | if command -v iconv >/dev/null 2>&1; then iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || cat; else cat; fi"
                               " | if command -v pbcopy >/dev/null 2>&1; then pbcopy; elif command -v wl-copy >/dev/null 2>&1; then wl-copy; elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard; elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --input; else printf 'No host clipboard backend found\\n' >&2; exit 127; fi");
}

#endif
