/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_capture.h"
#include "host_path.h"
#include "host_run_command.h"

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
    static char program[HOST_MAX_PATH_LEN];
    static char arguments[HOST_MAX_COMMAND_LEN];
    int windows_host;

    command[0] = '\0';
    program[0] = '\0';
    arguments[0] = '\0';

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

    windows_host = (GetHostPlatform() == HOST_PLATFORM_WINDOWS);

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

        if (windows_host) {
            /* argv[1] is the program, the rest is its Windows command line */
            if (i == 1 ? !host_append_literal(program, sizeof(program), target)
                       : !host_append_cmdline_arg(arguments, sizeof(arguments), target, i > 2)) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_append_shell_arg(command, sizeof(command),
                                          is_resolved_file ? filename : target, i > 1)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

    if (windows_host) {
        if (!host_append_run_command_windows(command, sizeof(command), program, arguments)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        if (host_print_command_output(command) != 0) {
            printf("Failed to run on host: %s\n", argv[1]);
            return HOST_RETURN_ERROR;
        }
        return 0;
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
