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

/*
 * Windows session command for argv[1..argc-1]: empty (interactive
 * cmd.exe) for no arguments, the argument itself for one, and the
 * arguments as quoted words for several. The terminal host runs it
 * through "cmd /S /C".
 */
static inline int host_append_shell_windows_command(char *command, size_t command_size,
                                                    int argc, char *argv[])
{
    if (argc == 2) {
        return host_append_literal(command, command_size, argv[1]);
    }

    for (int i = 1; i < argc; i++) {
        if (!host_append_cmd_word(command, command_size, argv[i], i > 1)) {
            return 0;
        }
    }
    return 1;
}

#endif
