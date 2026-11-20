/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_edit_command.h"
#include "host_launch.h"
#include "host_path.h"

static const char version[] = "$VER: Host-Edit " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Edit v%s\n", VERSION_STR);
    printf("Host-Edit opens files in the host desktop editor.\n");
    printf("%s\nUsage: host-edit <path> [path2 ...]\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static char filename[HOST_MAX_PATH_LEN];
    static char command[HOST_MAX_COMMAND_LEN];
    int status = 0;
    int windows_host;

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

    windows_host = (GetHostPlatform() == HOST_PLATFORM_WINDOWS);

    for (int i = 1; i < argc; i++) {
        const char *target = argv[i];
        int path_status;

        command[0] = '\0';

        if (argv[i][0] == '\0') {
            printf("Empty path argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        /* a file that does not exist yet is created in an existing directory */
        path_status = host_resolve_path(HOST_PATH_MODE_NEW, argv[i], filename,
                                        sizeof(filename), &target);
        if (path_status == HOST_PATH_URI) {
            printf("Cannot edit URI: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }
        if (!host_path_usable(path_status)) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (windows_host &&
            !host_path_utf8_target(argv[i], filename, sizeof(filename), &target)) {
            printf("Command is too long\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (windows_host
                ? !host_append_edit_command_windows(command, sizeof(command), target)
                : !host_append_edit_command(command, sizeof(command), target)) {
            printf("Command is too long\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (windows_host) {
            if (host_print_command_output(command) != 0) {
                printf("Failed to edit on host: %s\n", argv[i]);
                status = HOST_RETURN_ERROR;
            }
        } else if (host_launch_on_host(command, command) != 0) {
            printf("Failed to edit on host: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
        }
    }

    return status;
}
