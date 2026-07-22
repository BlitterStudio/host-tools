/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_ENV_COMMAND_H
#define HOST_ENV_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

static inline int host_env_is_alpha_or_underscore(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static inline int host_env_is_alnum_or_underscore(char c)
{
    return host_env_is_alpha_or_underscore(c) || (c >= '0' && c <= '9');
}

static inline int host_env_valid_name(const char *name)
{
    if (name == NULL || !host_env_is_alpha_or_underscore(name[0])) {
        return 0;
    }

    for (const char *p = name + 1; *p; p++) {
        if (!host_env_is_alnum_or_underscore(*p)) {
            return 0;
        }
    }
    return 1;
}

static inline int host_env_valid_value(const char *value)
{
    if (value == NULL) {
        return 0;
    }

    for (const char *p = value; *p; p++) {
        if (*p == '\n' || *p == '\r') {
            return 0;
        }
    }
    return 1;
}

static inline int host_append_env_export_line(char *line, size_t line_size,
                                              const char *name, const char *value)
{
    if (!host_env_valid_name(name) || !host_env_valid_value(value) ||
        !host_append_literal(line, line_size, "export ") ||
        !host_append_literal(line, line_size, name) ||
        !host_append_literal(line, line_size, "='")) {
        return 0;
    }

    for (const char *p = value; *p; p++) {
        char one[2];

        if (*p == '\'') {
            if (!host_append_literal(line, line_size, "'\\''")) {
                return 0;
            }
        } else {
            one[0] = *p;
            one[1] = '\0';
            if (!host_append_literal(line, line_size, one)) {
                return 0;
            }
        }
    }

    return host_append_literal(line, line_size, "'");
}

static inline int host_append_env_file_prefix(char *command, size_t command_size)
{
    return host_append_literal(command, command_size,
                               "umask 077; f=\"${HOME:?}/.host-tools-env\"; ");
}

static inline int host_append_env_get_command(char *command, size_t command_size,
                                              const char *name)
{
    if (!host_env_valid_name(name)) {
        return 0;
    }

    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size, "if [ \"${") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "+x}\" = x ]; then printf %s \"$") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "\"; elif [ -r \"$f\" ]; then . \"$f\"; if [ \"${") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "+x}\" = x ]; then printf %s \"$") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "\"; else exit 1; fi; else exit 1; fi");
}

static inline int host_append_env_set_command(char *command, size_t command_size,
                                              const char *name, const char *value)
{
    static char line[HOST_MAX_COMMAND_LEN];

    if (!host_env_valid_name(name) || !host_env_valid_value(value)) {
        return 0;
    }

    line[0] = '\0';
    if (!host_append_env_export_line(line, sizeof(line), name, value)) {
        return 0;
    }

    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "t=$(mktemp \"${f}.XXXXXX\") || exit 1; "
                               "trap 'rm -f \"$t\"' 0 1 2 15; "
                               "if [ -f \"$f\" ]; then grep -v '^export ") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size,
                               "=' \"$f\" > \"$t\"; s=$?; [ \"$s\" -le 1 ] || exit \"$s\"; fi; "
                               "printf '%s\\n' ") &&
           host_append_shell_arg(command, command_size, line, 0) &&
           host_append_literal(command, command_size,
                               " >> \"$t\" || exit $?; chmod 600 \"$t\" || exit $?; mv \"$t\" \"$f\"");
}

static inline int host_append_env_unset_command(char *command, size_t command_size,
                                                const char *name)
{
    if (!host_env_valid_name(name)) {
        return 0;
    }

    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "if [ -f \"$f\" ]; then t=$(mktemp \"${f}.XXXXXX\") || exit 1; "
                               "trap 'rm -f \"$t\"' 0 1 2 15; grep -v '^export ") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size,
                               "=' \"$f\" > \"$t\"; s=$?; [ \"$s\" -le 1 ] || exit \"$s\"; "
                               "chmod 600 \"$t\" || exit $?; mv \"$t\" \"$f\"; fi");
}

static inline int host_append_env_list_command(char *command, size_t command_size)
{
    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "if [ -r \"$f\" ]; then . \"$f\"; fi; env | sort");
}

static inline int host_append_env_get_command_windows(char *command, size_t command_size,
                                                      const char *name)
{
    static char script[HOST_MAX_COMMAND_LEN];

    if (!host_env_valid_name(name)) {
        return 0;
    }

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               "[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(28591);"
                               "$v=[Environment]::GetEnvironmentVariable(") &&
           host_append_ps_quoted(script, sizeof(script), name) &&
           host_append_literal(script, sizeof(script), ",'User');"
                               "if($null -eq $v){exit 1};[Console]::Out.Write($v)") &&
           host_append_ps_encoded_command(command, command_size, script);
}

static inline int host_append_env_set_command_windows(char *command, size_t command_size,
                                                      const char *name,
                                                      const char *value)
{
    static char script[HOST_MAX_COMMAND_LEN];

    if (!host_env_valid_name(name) || !host_env_valid_value(value)) {
        return 0;
    }

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               "[Environment]::SetEnvironmentVariable(") &&
           host_append_ps_quoted(script, sizeof(script), name) &&
           host_append_literal(script, sizeof(script), ",") &&
           host_append_ps_quoted(script, sizeof(script), value) &&
           host_append_literal(script, sizeof(script), ",'User')") &&
           host_append_ps_encoded_command(command, command_size, script);
}

static inline int host_append_env_unset_command_windows(char *command, size_t command_size,
                                                        const char *name)
{
    static char script[HOST_MAX_COMMAND_LEN];

    if (!host_env_valid_name(name)) {
        return 0;
    }

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               "[Environment]::SetEnvironmentVariable(") &&
           host_append_ps_quoted(script, sizeof(script), name) &&
           host_append_literal(script, sizeof(script), ",$null,'User')") &&
           host_append_ps_encoded_command(command, command_size, script);
}

static inline int host_append_env_list_command_windows(char *command, size_t command_size)
{
    return host_append_ps_encoded_command(command, command_size,
                                          "[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(28591);"
                                          "[Environment]::GetEnvironmentVariables('User').GetEnumerator()|"
                                          "Sort-Object Name|ForEach-Object{[Console]::Out.WriteLine(($_.Name)+'='+($_.Value))}");
}

#endif
