/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_env_command.h"

static const char version[] = "$VER: Host-Env " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Env v%s\n", VERSION_STR);
    printf("Host-Env gets and sets host user environment variables.\n");
    printf("%s\nUsage: host-env get <name>\n"
           "       host-env set <name> <value>\n"
           "       host-env unset <name>\n"
           "       host-env list\n", version);
    return 0;
}

static int require_name(const char *name)
{
    if (!host_env_valid_name(name)) {
        printf("Invalid environment variable name\n");
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    int windows;

    command[0] = '\0';

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing environment command\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    windows = (GetHostPlatform() == HOST_PLATFORM_WINDOWS);

    if (strcmp(argv[1], "get") == 0) {
        if (argc != 3) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!require_name(argv[2])) {
            return HOST_RETURN_ERROR;
        }
        if (windows) {
            if (!host_append_env_get_command_windows(command, sizeof(command), argv[2])) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_append_env_get_command(command, sizeof(command), argv[2])) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        return host_print_command_output(command);
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc != 4) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!require_name(argv[2])) {
            return HOST_RETURN_ERROR;
        }
        if (windows) {
            if (!host_append_env_set_command_windows(command, sizeof(command), argv[2], argv[3])) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_append_env_set_command(command, sizeof(command), argv[2], argv[3])) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        return host_print_command_output(command);
    }

    if (strcmp(argv[1], "unset") == 0) {
        if (argc != 3) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!require_name(argv[2])) {
            return HOST_RETURN_ERROR;
        }
        if (windows) {
            if (!host_append_env_unset_command_windows(command, sizeof(command), argv[2])) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_append_env_unset_command(command, sizeof(command), argv[2])) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        return host_print_command_output(command);
    }

    if (strcmp(argv[1], "list") == 0) {
        if (argc != 2) {
            printf("Unexpected argument after list\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (windows) {
            if (!host_append_env_list_command_windows(command, sizeof(command))) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_append_env_list_command(command, sizeof(command))) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        return host_print_command_output(command);
    }

    printf("Unknown environment command\n");
    print_usage();
    return HOST_RETURN_ERROR;
}
