/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_info_command.h"

static const char version[] = "$VER: Host-Info " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Info v%s\n", VERSION_STR);
    printf("Host-Info prints basic host integration details.\n");
    printf("%s\nUsage: host-info\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc == 2 && strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (argc > 1)
    {
        printf("Unexpected argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    return host_print_command_output(HOST_INFO_COMMAND);
}
