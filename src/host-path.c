/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_path.h"

static const char version[] = "$VER: Host-Path v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Path v%s\n", VERSION_STR);
    printf("Host-Path prints host paths for Amiga paths.\n");
    printf("%s\nUsage: host-path <path> [path2 ...]\n", version);
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
        printf("Missing path argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++) {
        int path_status = host_resolve_existing_path(argv[i], filename, sizeof(filename));
        if (path_status == HOST_PATH_OK) {
            printf("%s\n", filename);
        } else {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            status = HOST_RETURN_ERROR;
        }
    }

    return status;
}
