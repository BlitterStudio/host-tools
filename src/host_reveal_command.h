/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_REVEAL_COMMAND_H
#define HOST_REVEAL_COMMAND_H

#include <stddef.h>
#include <string.h>
#include "host_common.h"
#include "host_powershell.h"

/*
 * Windows: Explorer selects the file directly. Explorer returns exit
 * code 1 even when it works, and Start-Process does not wait for it, so
 * the script's status only says whether Explorer could be started. The
 * path is ISO-8859-1 and goes through PowerShell, not cmd.exe, which would
 * expand percent signs and read it in the ANSI code page. A path with a
 * quote cannot be a Windows path and is rejected.
 */
static inline int host_append_reveal_command_windows(char *command, size_t command_size,
                                                     const char *path)
{
    static char script[HOST_MAX_COMMAND_LEN];
    static char argument[HOST_MAX_COMMAND_LEN];

    if (strchr(path, '"') != NULL) {
        return 0;
    }

    script[0] = '\0';
    argument[0] = '\0';
    return host_append_literal(argument, sizeof(argument), "/select,\"") &&
           host_append_literal(argument, sizeof(argument), path) &&
           host_append_literal(argument, sizeof(argument), "\"") &&
           host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "Start-Process -FilePath 'explorer.exe' -ArgumentList ") &&
           host_append_ps_quoted(script, sizeof(script), argument) &&
           host_append_ps_encoded_command(command, command_size, script);
}

static inline int host_append_show_items_branch(char *command, size_t command_size,
                                                const char *uri)
{
    return host_append_literal(command, command_size,
                              "; elif command -v gdbus >/dev/null 2>&1 && gdbus call --session"
                              " --dest org.freedesktop.FileManager1"
                              " --object-path /org/freedesktop/FileManager1"
                              " --method org.freedesktop.FileManager1.ShowItems ") &&
           host_append_literal(command, command_size, "'[\"") &&
           host_append_literal(command, command_size, uri) &&
           host_append_literal(command, command_size, "\"]' '' >/dev/null 2>&1; then :");
}

static inline int host_append_reveal_command(char *command, size_t command_size,
                                             const char *path)
{
    static char uri[HOST_FILE_URI_MAX_LEN];
    int have_uri = host_path_to_file_uri(path, uri, sizeof(uri));

    return host_append_literal(command, command_size,
                              "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then open -R ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           (!have_uri || host_append_show_items_branch(command, command_size, uri)) &&
           host_append_literal(command, command_size,
                               "; elif command -v xdg-open >/dev/null 2>&1; then xdg-open \"$(dirname -- ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size,
                               ")\"; else exit 127; fi");
}

#endif
