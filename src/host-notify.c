/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_notify_command.h"

static const char version[] = "$VER: Host-Notify " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Notify v%s\n", VERSION_STR);
    printf("Host-Notify sends a desktop notification on the host.\n");
    printf("%s\nUsage: host-notify <message>\n       host-notify <title> <message...>\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char title[512];
    static char message[2048];

    command[0] = '\0';
    title[0] = '\0';
    host_append_literal(title, sizeof(title), "Amiga");
    message[0] = '\0';

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing message argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (argc == 2) {
        if (!host_append_literal(message, sizeof(message), argv[1])) {
            printf("Message is too long\n");
            return HOST_RETURN_ERROR;
        }
    } else {
        title[0] = '\0';
        if (!host_append_literal(title, sizeof(title), argv[1])) {
            printf("Title is too long\n");
            return HOST_RETURN_ERROR;
        }
        message[0] = '\0';
        if (!host_join_args(message, sizeof(message), argc, argv, 2)) {
            printf("Message is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

    if (!host_append_notify_command(command, sizeof(command), title, message)) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    if (host_print_command_output(command) != 0) {
        printf("Failed to send host notification\n");
        return HOST_RETURN_ERROR;
    }

    return 0;
}
