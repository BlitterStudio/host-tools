/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_NOTIFY_COMMAND_H
#define HOST_NOTIFY_COMMAND_H

#include <stddef.h>
#include "host_common.h"

static inline int host_append_notify_command(char *command, size_t command_size,
                                             const char *title, const char *message)
{
    return host_append_literal(command, command_size, "t=") &&
           host_append_shell_arg(command, command_size, title, 0) &&
           host_append_literal(command, command_size, "; m=") &&
           host_append_shell_arg(command, command_size, message, 0) &&
           host_append_literal(command, command_size,
                               "; if command -v iconv >/dev/null 2>&1; then"
                               " t=$(printf %s \"$t\" | iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || printf %s \"$t\");"
                               " m=$(printf %s \"$m\" | iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || printf %s \"$m\"); fi"
                               "; if command -v notify-send >/dev/null 2>&1; then notify-send \"$t\" \"$m\""
                               "; elif command -v osascript >/dev/null 2>&1; then osascript -e 'on run argv' -e 'display notification (item 2 of argv) with title (item 1 of argv)' -e 'end run' \"$t\" \"$m\""
                               "; else exit 127; fi");
}

#endif
