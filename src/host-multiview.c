/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_common.h"
#include "uae_pragmas.h"

static const char version[] = "$VER: Host-MultiView v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-MultiView v%s\n", VERSION_STR);
    printf("Host-MultiView is a command line tool to open files or URLs with the host default handler, from within UAE.\n");
    printf("%s\nUsage: host-multiview <filename|URL> [filename2|URL2 ...]\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    BPTR lock;
    char filename[HOST_MAX_PATH_LEN];
    int status = 0;

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing filename or URL argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    /* Iterate through all arguments and request the host to view them */
    for (int i = 1; i < argc; i++)
    {
        char *target = argv[i];

        if (argv[i][0] == '\0') {
            printf("Empty filename or URL argument\n");
            status = HOST_RETURN_ERROR;
            continue;
        }

        /* Try to resolve as a file path first to get the host path (skip URLs) */
        if (!host_is_uri(argv[i]) && ((lock = Lock((STRPTR)argv[i], ACCESS_READ))))
        {
            filename[0] = '\0';
            filename[sizeof(filename) - 1] = '\0';
            if (NativeDosOp(0, (ULONG)lock, (ULONG)filename, sizeof(filename)) == 0) {
                 UnLock(lock);
                 if (host_filled_buffer(filename, sizeof(filename))) {
                     printf("Resolved host path is too long: %s\n", argv[i]);
                     status = HOST_RETURN_ERROR;
                     continue;
                 }
                 target = filename;
            } else {
                 UnLock(lock);
            }
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
