/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

        int path_status = host_resolve_optional_path(argv[i], filename, sizeof(filename),
                                                     (const char **)&target);
        if (path_status != HOST_PATH_OK) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
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
