/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"

static const char version[] = "$VER: Host-Notify v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Notify v%s\n", VERSION_STR);
    printf("Host-Notify sends a desktop notification on the host.\n");
    printf("%s\nUsage: host-notify <message>\n       host-notify <title> <message...>\n", version);
    return 0;
}

static int append_notify_command(char *command, size_t command_size,
                                 const char *title, const char *message)
{
    return host_append_literal(command, command_size,
                              "if command -v notify-send >/dev/null 2>&1; then notify-send ") &&
           host_append_shell_arg(command, command_size, title, 0) &&
           host_append_shell_arg(command, command_size, message, 1) &&
           host_append_literal(command, command_size,
                               "; elif command -v osascript >/dev/null 2>&1; then osascript -e 'on run argv' -e 'display notification (item 2 of argv) with title (item 1 of argv)' -e 'end run' ") &&
           host_append_shell_arg(command, command_size, title, 0) &&
           host_append_shell_arg(command, command_size, message, 1) &&
           host_append_literal(command, command_size,
                               "; else exit 127; fi");
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char title[512];
    static char message[2048];

    command[0] = '\0';
    strcpy(title, "Amiga");
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

    if (!append_notify_command(command, sizeof(command), title, message)) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    if (host_print_command_output(command) != 0) {
        printf("Failed to send host notification\n");
        return HOST_RETURN_ERROR;
    }

    return 0;
}
