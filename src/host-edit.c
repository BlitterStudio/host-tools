/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_path.h"

static const char version[] = "$VER: Host-Edit v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Edit v%s\n", VERSION_STR);
    printf("Host-Edit opens files in the host desktop editor.\n");
    printf("%s\nUsage: host-edit <path> [path2 ...]\n", version);
    return 0;
}

static int append_file_uri_arg(char *command, size_t command_size, const char *path)
{
    static const char hex[] = "0123456789ABCDEF";
    static char uri[HOST_MAX_PATH_LEN * 3 + 8];
    size_t pos = 0;

    if (path[0] != '/') {
        return 0;
    }

    memcpy(uri, "file://", 7);
    pos = 7;

    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == '-' ||
            *p == '.' || *p == '_' || *p == '~') {
            if (pos + 1 >= sizeof(uri)) {
                return 0;
            }
            uri[pos++] = (char)*p;
        } else {
            if (pos + 3 >= sizeof(uri)) {
                return 0;
            }
            uri[pos++] = '%';
            uri[pos++] = hex[*p >> 4];
            uri[pos++] = hex[*p & 0x0f];
        }
    }

    uri[pos] = '\0';
    return host_append_shell_arg(command, command_size, uri, 0);
}

static int append_edit_command(char *command, size_t command_size, const char *path)
{
    if (!host_append_literal(command, command_size,
                             "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then open -t ") ||
        !host_append_shell_arg(command, command_size, path, 0) ||
        !host_append_literal(command, command_size,
                             "; else mime_type=text/plain; desktop_file=; if command -v xdg-mime >/dev/null 2>&1; then detected_mime=$(xdg-mime query filetype ") ||
        !host_append_shell_arg(command, command_size, path, 0) ||
        !host_append_literal(command, command_size,
                             " 2>/dev/null); [ -n \"$detected_mime\" ] && mime_type=$detected_mime; desktop_file=$(xdg-mime query default \"$mime_type\" 2>/dev/null); [ -n \"$desktop_file\" ] || desktop_file=$(xdg-mime query default text/plain 2>/dev/null); fi; ")) {
        return 0;
    }

    if (path[0] == '/') {
        if (!host_append_literal(command, command_size,
                                 "if [ -n \"$desktop_file\" ] && command -v gtk-launch >/dev/null 2>&1 && gtk-launch \"$desktop_file\" ") ||
            !append_file_uri_arg(command, command_size, path) ||
            !host_append_literal(command, command_size, "; then :; elif ")) {
            return 0;
        }
    } else if (!host_append_literal(command, command_size, "if false; then :; elif ")) {
        return 0;
    }

    return host_append_literal(command, command_size,
                               "command -v xdg-open >/dev/null 2>&1 && xdg-open ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size,
                               "; then :; else host_editor=${VISUAL:-${EDITOR:-}}; if [ -n \"$host_editor\" ]; then HOST_EDITOR=\"$host_editor\" sh -c 'exec $HOST_EDITOR \"$1\"' sh ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size,
                               "; else exit 127; fi; fi; fi");
}

int main(int argc, char *argv[])
{
    static char filename[HOST_MAX_PATH_LEN];
    static char command[HOST_MAX_COMMAND_LEN];
    int status = 0;

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing path argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++) {
        const char *target = argv[i];
        int path_status;

        command[0] = '\0';

        if (argv[i][0] == '\0') {
            printf("Empty path argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (host_is_uri(argv[i])) {
            printf("Cannot edit URI: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        path_status = host_resolve_optional_path(argv[i], filename, sizeof(filename), &target);
        if (path_status != HOST_PATH_OK) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (!append_edit_command(command, sizeof(command), target)) {
            printf("Command is too long\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (!ExecuteOnHost((UBYTE *)command)) {
            printf("Failed to edit on host: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
        }
    }

    return status;
}
