/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_path.h"

static const char version[] = "$VER: Host-Reveal v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Reveal v%s\n", VERSION_STR);
    printf("Host-Reveal reveals files in the host file manager.\n");
    printf("%s\nUsage: host-reveal <path> [path2 ...]\n", version);
    return 0;
}

static int append_reveal_command(char *command, size_t command_size, const char *path)
{
    return host_append_literal(command, command_size,
                              "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then open -R ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size,
                               "; elif command -v xdg-open >/dev/null 2>&1; then xdg-open \"$(dirname -- ") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size,
                               ")\"; else exit 127; fi");
}

int main(int argc, char *argv[])
{
    char filename[HOST_MAX_PATH_LEN];
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
        char command[HOST_MAX_COMMAND_LEN] = "";
        const char *target = argv[i];
        int path_status;

        if (argv[i][0] == '\0') {
            printf("Empty path argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (host_is_uri(argv[i])) {
            printf("Cannot reveal URI: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        path_status = host_resolve_optional_path(argv[i], filename, sizeof(filename), &target);
        if (path_status != HOST_PATH_OK) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (!append_reveal_command(command, sizeof(command), target)) {
            printf("Command is too long\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (host_print_command_output(command) != 0) {
            printf("Failed to reveal on host: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
        }
    }

    return status;
}
