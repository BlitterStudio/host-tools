/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CLIP_COMMAND_H
#define HOST_CLIP_COMMAND_H

#include <stddef.h>
#include "host_common.h"

#define HOST_CLIP_PASTE_COMMAND \
    "if command -v pbpaste >/dev/null 2>&1; then pbpaste; " \
    "elif command -v wl-paste >/dev/null 2>&1; then wl-paste -n; " \
    "elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard -o; " \
    "elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --output; " \
    "else printf 'No host clipboard backend found\\n' >&2; exit 127; fi"

static inline int host_append_clip_copy_command(char *command, size_t command_size,
                                                const char *text)
{
    return host_append_literal(command, command_size, "printf %s ") &&
           host_append_shell_arg(command, command_size, text, 0) &&
           host_append_literal(command, command_size,
                               " | if command -v pbcopy >/dev/null 2>&1; then pbcopy; elif command -v wl-copy >/dev/null 2>&1; then wl-copy; elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard; elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --input; else printf 'No host clipboard backend found\\n' >&2; exit 127; fi");
}

#endif
