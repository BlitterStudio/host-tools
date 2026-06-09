/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_path.h"

static const char version[] = "$VER: Host-Run " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Run v%s\n", VERSION_STR);
    printf("Host-Run is a command line tool to run host commands from within UAE.\n");
    printf("%s\nUsage: host-run <command> <argument1> <argument2> ...\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char filename[HOST_MAX_PATH_LEN];

    command[0] = '\0';

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++)
    {
        // Try to resolve as a file path first (skip URLs to avoid volume requester)
        int is_resolved_file = 0;
        const char *target = argv[i];
        int path_status = host_resolve_optional_path(argv[i], filename, sizeof(filename), &target);
        if (path_status != HOST_PATH_OK) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            return HOST_RETURN_ERROR;
        }
        is_resolved_file = (target == filename);

        if (!host_append_shell_arg(command, sizeof(command),
                                   is_resolved_file ? filename : target, i > 1)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

#ifdef DEBUG
    printf("DEBUG: argc=%d, command=%s\n", argc, command);
#endif
    if (!ExecuteOnHost((UBYTE *)command)) {
        printf("Failed to execute command on host\n");
        return HOST_RETURN_ERROR;
    }
    return 0;
}
