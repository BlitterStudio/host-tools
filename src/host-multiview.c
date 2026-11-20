/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_capture.h"
#include "host_multiview_command.h"
#include "host_path.h"

static const char version[] = "$VER: Host-MultiView " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-MultiView v%s\n", VERSION_STR);
    printf("Host-MultiView is a command line tool to open files or URLs with the host default handler, from within UAE.\n");
    printf("%s\nUsage: host-multiview <filename|URL> [filename2|URL2 ...]\n", version);
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
        printf("Missing filename or URL argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    windows_host = (GetHostPlatform() == HOST_PLATFORM_WINDOWS);

    /* Iterate through all arguments and request the host to view them */
    for (int i = 1; i < argc; i++)
    {
        const char *target = argv[i];

        if (argv[i][0] == '\0') {
            printf("Empty filename or URL argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        int path_status = host_resolve_path(HOST_PATH_MODE_FILE, argv[i], filename,
                                            sizeof(filename), &target);
        if (!host_path_usable(path_status)) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
            continue;
        }

        if (windows_host) {
            if (!host_path_utf8_target(argv[i], filename, sizeof(filename),
                                       &target)) {
                printf("Command is too long\n");
                status = HOST_RETURN_ERROR;
                continue;
            }
            command[0] = '\0';
            if (!host_append_multiview_command_windows(command, sizeof(command), target)) {
                printf("Command is too long\n");
                status = HOST_RETURN_ERROR;
            } else if (host_print_command_output(command) != 0) {
                printf("Failed to open on host: %s\n", argv[i]);
                status = HOST_RETURN_ERROR;
            }
            continue;
        }

        /* Send the request to Amiberry */
        /* Opcode 89 handles the quoting and OS-specific command (open/xdg-open) */
        if (!HostShell_View((UBYTE *)target)) {
            printf("Failed to open on host: %s\n", argv[i]);
            status = HOST_RETURN_ERROR;
        }
    }
    
    return status;
}
