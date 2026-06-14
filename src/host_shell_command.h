/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_SHELL_COMMAND_H
#define HOST_SHELL_COMMAND_H

#include <stddef.h>

#include "host_common.h"

static inline int host_append_shell_login_command(char *command, size_t command_size,
                                                  const char *session_command)
{
    if (!host_append_literal(command, command_size, "h=\"${SHELL:-/bin/sh}\"; exec \"$h\" -l")) {
        return 0;
    }

    if (session_command == NULL || session_command[0] == '\0') {
        return 1;
    }

    return host_append_literal(command, command_size, " -c ") &&
           host_append_shell_arg(command, command_size, session_command, 0);
}

#endif
