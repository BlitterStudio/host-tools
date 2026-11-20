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

static inline int host_is_safe_cmd_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '/' ||
           c == '\\' || c == ':' || c == '+' || c == '@' || c == '-';
}

/*
 * Quotes and line breaks cannot be passed through cmd.exe safely, and a
 * percent sign would still expand an environment variable inside quotes.
 */
static inline int host_cmd_word_valid(const char *word)
{
    for (const char *p = word; *p; p++) {
        if (*p == '"' || *p == '%' || *p == '\n' || *p == '\r') {
            return 0;
        }
    }
    return 1;
}

/*
 * Append one word to a cmd.exe command line, in double quotes when it is
 * empty or holds anything but plain path characters. Words that
 * host_cmd_word_valid rejects fail.
 */
static inline int host_append_cmd_word(char *command, size_t command_size, const char *word,
                                       int add_separator)
{
    int quote = (word[0] == '\0');

    if (!host_cmd_word_valid(word)) {
        return 0;
    }
    for (const char *p = word; *p; p++) {
        if (!host_is_safe_cmd_char(*p)) {
            quote = 1;
        }
    }

    return (!add_separator || host_append_literal(command, command_size, " ")) &&
           (!quote || host_append_literal(command, command_size, "\"")) &&
           host_append_literal(command, command_size, word) &&
           (!quote || host_append_literal(command, command_size, "\""));
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
