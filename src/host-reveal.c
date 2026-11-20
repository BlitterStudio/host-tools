/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_path.h"
#include "host_reveal_command.h"

static const char version[] = "$VER: Host-Reveal " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Reveal v%s\n", VERSION_STR);
    printf("Host-Reveal reveals files in the host file manager.\n");
    printf("%s\nUsage: host-reveal <path> [path2 ...]\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static char filename[HOST_MAX_PATH_LEN];
    static char utf8[HOST_MAX_PATH_LEN * 2];
    static char command[HOST_MAX_COMMAND_LEN];
    int status = 0;
    int platform;

    if (argc >= 2 && strcmp(argv[1], "?") == 0)
    {
        if (argc > 2)
        {
            printf("Unexpected argument after ?\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        return print_usage();
    }

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing path argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    platform = GetHostPlatform();

    for (int i = 1; i < argc; i++) {
        const char *target = argv[i];
        int path_status;

        command[0] = '\0';

        if (argv[i][0] == '\0') {
            printf("Empty path argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        path_status = host_resolve_path(HOST_PATH_MODE_FILE, argv[i], filename,
                                        sizeof(filename), &target);
        if (path_status == HOST_PATH_URI) {
            printf("Cannot reveal URI: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }
        if (!host_path_usable(path_status)) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        /* paths and text are ISO-8859-1 on the Amiga; POSIX hosts want UTF-8
         * (the Windows script builders convert as they quote) */
        if (platform != HOST_PLATFORM_WINDOWS &&
            !host_path_to_utf8_target(utf8, sizeof(utf8), &target)) {
            printf("Command is too long\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (platform == HOST_PLATFORM_WINDOWS
                ? !host_append_reveal_command_windows(command, sizeof(command), target)
                : !host_append_reveal_command(command, sizeof(command), target)) {
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
