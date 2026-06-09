/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_REVEAL_COMMAND_H
#define HOST_REVEAL_COMMAND_H

#include <stddef.h>
#include "host_common.h"

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
