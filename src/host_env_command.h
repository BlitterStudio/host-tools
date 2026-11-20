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

/*
 * The scripts keep their own state in the positional parameters ($1 is
 * the environment file, $2 the lock directory when one is held, $3 the
 * temporary file) rather than in named variables, because "get" looks
 * up a variable chosen by the user: a scratch variable called f or t
 * would shadow the user's.
 */
static inline int host_append_env_file_prefix(char *command, size_t command_size)
{
    return host_append_literal(command, command_size,
                               "umask 077; set -- \"${HOME:?}/.host-tools-env\" '' ''; ");
}

/*
 * Serialize read/modify/write of the environment file: two host-env commands
 * running set or unset at once would otherwise each read the old file
 * and the later rename would drop the earlier change. flock is used
 * where it exists (the lock goes away with the process); otherwise a
 * lock directory created with mkdir, removed on exit and on signals.
 * It waits up to five seconds.
 */
static inline int host_append_env_lock_prefix(char *command, size_t command_size)
{
    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "trap 'rm -f \"$3\"; [ -z \"$2\" ] || rmdir \"$2\"' 0; "
                               "trap 'exit 1' 1 2 15; n=0; "
                               "if command -v flock >/dev/null 2>&1; then "
                               "exec 9>\"$1.lock\" || exit 1; "
                               "until flock -n 9; do n=$((n+1)); "
                               "if [ \"$n\" -ge 50 ]; then echo 'Cannot lock the environment file'; exit 1; fi; "
                               "sleep 0.1 2>/dev/null || sleep 1; done; "
                               "else "
                               "until mkdir \"$1.lock.d\" 2>/dev/null; do n=$((n+1)); "
                               "if [ \"$n\" -ge 50 ]; then echo \"Cannot lock the environment file (remove $1.lock.d if no other host-env is running)\"; exit 1; fi; "
                               "sleep 0.1 2>/dev/null || sleep 1; done; "
                               "set -- \"$1\" \"$1.lock.d\" ''; fi; ");
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
           host_append_literal(command, command_size, "+x}\" = x ]; then printf '%s\\n' \"$") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "\"; elif [ -r \"$1\" ]; then . \"$1\"; if [ \"${") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size, "+x}\" = x ]; then printf '%s\\n' \"$") &&
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

    return host_append_env_lock_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "t=$(mktemp \"$1.XXXXXX\") || exit 1; set -- \"$1\" \"$2\" \"$t\"; "
                               "if [ -f \"$1\" ]; then grep -v '^export ") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size,
                               "=' \"$1\" > \"$3\"; s=$?; [ \"$s\" -le 1 ] || exit \"$s\"; fi; "
                               "printf '%s\\n' ") &&
           host_append_shell_arg(command, command_size, line, 0) &&
           host_append_literal(command, command_size,
                               " >> \"$3\" || exit $?; chmod 600 \"$3\" || exit $?; mv \"$3\" \"$1\"");
}

static inline int host_append_env_unset_command(char *command, size_t command_size,
                                                const char *name)
{
    if (!host_env_valid_name(name)) {
        return 0;
    }

    return host_append_env_lock_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "if [ -f \"$1\" ]; then t=$(mktemp \"$1.XXXXXX\") || exit 1; "
                               "set -- \"$1\" \"$2\" \"$t\"; grep -v '^export ") &&
           host_append_literal(command, command_size, name) &&
           host_append_literal(command, command_size,
                               "=' \"$1\" > \"$3\"; s=$?; [ \"$s\" -le 1 ] || exit \"$s\"; "
                               "chmod 600 \"$3\" || exit $?; mv \"$3\" \"$1\"; fi");
}

static inline int host_append_env_list_command(char *command, size_t command_size)
{
    return host_append_env_file_prefix(command, command_size) &&
           host_append_literal(command, command_size,
                               "if [ -r \"$1\" ]; then . \"$1\"; fi; env | sort");
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
                               "if($null -eq $v){exit 1};[Console]::Out.WriteLine($v)") &&
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
