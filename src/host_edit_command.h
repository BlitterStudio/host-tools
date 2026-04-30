/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_EDIT_COMMAND_H
#define HOST_EDIT_COMMAND_H

#include <stddef.h>
#include <string.h>
#include "host_common.h"

#define HOST_FILE_URI_MAX_LEN (HOST_MAX_PATH_LEN * 3 + 8)
#define HOST_EDIT_COMMAND_TAIL "; if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then open -t \"$target\"; else mime_type=text/plain; desktop_file=; if command -v xdg-mime >/dev/null 2>&1; then detected_mime=$(xdg-mime query filetype \"$target\" 2>/dev/null); [ -n \"$detected_mime\" ] && mime_type=$detected_mime; desktop_file=$(xdg-mime query default \"$mime_type\" 2>/dev/null); [ -n \"$desktop_file\" ] || desktop_file=$(xdg-mime query default text/plain 2>/dev/null); fi; if [ -n \"$target_uri\" ] && [ -n \"$desktop_file\" ] && command -v gtk-launch >/dev/null 2>&1 && gtk-launch \"$desktop_file\" \"$target_uri\"; then :; elif command -v xdg-open >/dev/null 2>&1 && xdg-open \"$target\"; then :; else host_editor=${VISUAL:-${EDITOR:-}}; if [ -n \"$host_editor\" ]; then HOST_EDITOR=\"$host_editor\" sh -c 'exec $HOST_EDITOR \"$1\"' sh \"$target\"; else exit 127; fi; fi; fi"

static inline int host_path_to_file_uri(const char *path, char *uri, size_t uri_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;

    if (path == NULL || uri == NULL || uri_size < 8 || path[0] != '/') {
        return 0;
    }

    memcpy(uri, "file://", 7);
    pos = 7;

    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == '-' ||
            *p == '.' || *p == '_' || *p == '~') {
            if (pos + 1 >= uri_size) {
                return 0;
            }
            uri[pos++] = (char)*p;
        } else {
            if (pos + 3 >= uri_size) {
                return 0;
            }
            uri[pos++] = '%';
            uri[pos++] = hex[*p >> 4];
            uri[pos++] = hex[*p & 0x0f];
        }
    }

    uri[pos] = '\0';
    return 1;
}

static inline int host_append_edit_command_with_uri(char *command, size_t command_size,
                                                   const char *path, const char *uri)
{
    return host_append_literal(command, command_size, "target=") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size, "; target_uri=") &&
           (uri[0] == '\0' || host_append_shell_arg(command, command_size, uri, 0)) &&
           host_append_literal(command, command_size, HOST_EDIT_COMMAND_TAIL);
}

static inline int host_append_edit_command(char *command, size_t command_size, const char *path)
{
    static char uri[HOST_FILE_URI_MAX_LEN];
    size_t original_len;

    if (command == NULL || command_size == 0 || path == NULL) {
        return 0;
    }

    original_len = strlen(command);
    if (original_len >= command_size) {
        return 0;
    }

    if (host_path_to_file_uri(path, uri, sizeof(uri))) {
        if (host_append_edit_command_with_uri(command, command_size, path, uri)) {
            return 1;
        }
        command[original_len] = '\0';
    }

    return host_append_edit_command_with_uri(command, command_size, path, "");
}

#endif
