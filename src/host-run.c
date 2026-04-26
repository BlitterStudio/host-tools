/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_common.h"
#include "uae_pragmas.h"

static const char version[] = "$VER: Host-Run v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Run v%s\n", VERSION_STR);
    printf("Host-Run is a command line tool to run host commands from within UAE.\n");
    printf("%s\nUsage: host-run <command> <argument1> <argument2> ...\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    BPTR lock;
    char command[HOST_MAX_COMMAND_LEN] = "";
    char filename[HOST_MAX_PATH_LEN];

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++)
    {
        // Try to resolve as a file path first (skip URLs to avoid volume requester)
        int is_resolved_file = 0;
        if (argv[i][0] != '\0' && !host_is_uri(argv[i]) && ((lock = Lock((STRPTR)argv[i], ACCESS_READ))))
        {
            filename[0] = '\0';
            filename[sizeof(filename) - 1] = '\0';
            if (NativeDosOp(0, (ULONG)lock, (ULONG)filename, sizeof(filename)) == 0) {
                 UnLock(lock);
                 if (host_filled_buffer(filename, sizeof(filename))) {
                     printf("Resolved host path is too long\n");
                     return HOST_RETURN_ERROR;
                 }
                 is_resolved_file = 1;
            } else {
                 UnLock(lock);
            }
        }

        if (!host_append_shell_arg(command, sizeof(command),
                                   is_resolved_file ? filename : argv[i], i > 1)) {
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
