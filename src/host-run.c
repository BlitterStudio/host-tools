/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "host_capture.h"
#include "host_launch.h"
#include "host_path.h"
#include "host_run_command.h"

static const char version[] = "$VER: Host-Run " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Run v%s\n", VERSION_STR);
    printf("Host-Run is a command line tool to run host commands from within UAE.\n");
    printf("%s\nUsage: host-run <command> <argument1> <argument2> ...\n", version);
    printf("Arguments that name existing Amiga files become host paths.\n");
    return 0;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char plain[HOST_MAX_COMMAND_LEN];
    static char filename[HOST_MAX_PATH_LEN];
    static char utf8[HOST_MAX_PATH_LEN * 2];
    static char program[HOST_MAX_PATH_LEN];
    static char arguments[HOST_MAX_COMMAND_LEN];
    static char crt_arguments[HOST_MAX_COMMAND_LEN];
    int windows_host;
    int batch_safe = 1;

    command[0] = '\0';
    plain[0] = '\0';
    program[0] = '\0';
    arguments[0] = '\0';
    crt_arguments[0] = '\0';

    if (argc >= 2 && strcmp(argv[1], "?") == 0)
    {
        if (argc > 2)
        {
            printf("Unexpected argument after ?\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        return print_usage();
    }

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

    windows_host = (GetHostPlatform() == HOST_PLATFORM_WINDOWS);

    for (int i = 1; i < argc; i++)
    {
        /* An argument that names an Amiga file becomes its host path; a
         * path on a known volume that does not exist is an error. Anything
         * else (flags, URLs, host paths) goes through as written. */
        const char *target = argv[i];
        int path_status = host_resolve_path(HOST_PATH_MODE_ARG, argv[i], filename,
                                            sizeof(filename), &target);
        if (!host_path_usable(path_status)) {
            printf("%s: %s\n", host_path_error(path_status), argv[i]);
            return HOST_RETURN_ERROR;
        }

        if (windows_host) {
            /* argv[1] is the program, the rest is its Windows command line;
             * the script builder converts ISO-8859-1 to UTF-8 as it quotes */
            if (i == 1) {
                if (!host_append_literal(program, sizeof(program), target)) {
                    printf("Command is too long\n");
                    return HOST_RETURN_ERROR;
                }
                continue;
            }
            /* cmd.exe words suit batch files and native programs; arguments
             * they cannot carry fall back to C runtime quoting, which
             * must not reach a batch file */
            if (batch_safe && !host_append_cmd_word(arguments, sizeof(arguments), target, i > 2)) {
                batch_safe = 0;
            }
            if (!host_append_cmdline_arg(crt_arguments, sizeof(crt_arguments), target, i > 2)) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        } else if (!host_path_to_utf8_target(utf8, sizeof(utf8), &target) ||
                   (i == 1
                       ? !host_append_shell_arg(program, sizeof(program), target, 0)
                       : !host_append_shell_arg(arguments, sizeof(arguments), target, 1))) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

    if (windows_host) {
        if (!host_append_run_command_windows(command, sizeof(command), program,
                                             batch_safe ? arguments : crt_arguments,
                                             batch_safe)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        if (host_print_command_output(command) != 0) {
            printf("Failed to run on host: %s\n", argv[1]);
            return HOST_RETURN_ERROR;
        }
        return 0;
    }

    if (!host_append_run_command(command, sizeof(command), program, arguments) ||
        !host_append_literal(plain, sizeof(plain), program) ||
        !host_append_literal(plain, sizeof(plain), arguments)) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }
#ifdef DEBUG
    printf("DEBUG: argc=%d, command=%s\n", argc, command);
#endif
    if (host_launch_on_host(command, plain) != 0) {
        printf("Failed to run on host: %s\n", argv[1]);
        return HOST_RETURN_ERROR;
    }
    return 0;
}
