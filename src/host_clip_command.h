/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CLIP_COMMAND_H
#define HOST_CLIP_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

#define HOST_CLIP_PASTE_COMMAND \
    "out=$(if command -v pbpaste >/dev/null 2>&1; then pbpaste; " \
    "elif command -v wl-paste >/dev/null 2>&1; then wl-paste -n; " \
    "elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard -o; " \
    "elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --output; " \
    "else printf 'No host clipboard backend found\\n' >&2; exit 127; fi) || exit $?; " \
    "printf %s \"$out\" | if command -v iconv >/dev/null 2>&1; then " \
    "iconv -c -f UTF-8 -t ISO-8859-1//TRANSLIT 2>/dev/null || cat; else cat; fi"

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
