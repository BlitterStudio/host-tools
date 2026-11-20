/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_say_command.h"

static const char version[] = "$VER: Host-Say " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Say v%s\n", VERSION_STR);
    printf("Host-Say speaks text through the host speech synthesizer.\n");
    printf("%s\nUsage: host-say <text...>\n"
           "       host-say < file (reads standard input)\n", version);
    return 0;
}

static int read_stdin_text(char *text, size_t text_size)
{
    size_t total = fread(text, 1, text_size - 1, stdin);

    text[total] = '\0';
    if (total == text_size - 1 && getc(stdin) != EOF) {
        return 0;
    }
    return 1;
}

static int has_speakable_text(const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p > ' ') {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char text[2048];

    command[0] = '\0';
    text[0] = '\0';

    if (argc > 1 && strcmp(argv[1], "?") == 0)
    {
        if (argc > 2) {
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

    if (argc <= 1) {
        /* without text arguments the text comes from standard input,
         * but a console would only wait for the user to type */
        if (IsInteractive(Input())) {
            printf("Missing text argument\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!read_stdin_text(text, sizeof(text))) {
            printf("Text is too long\n");
            return HOST_RETURN_ERROR;
        }
    } else if (!host_join_args(text, sizeof(text), argc, argv, 1)) {
        printf("Text is too long\n");
        return HOST_RETURN_ERROR;
    }

    if (!has_speakable_text(text)) {
        printf("Nothing to say\n");
        return HOST_RETURN_ERROR;
    }

    if (GetHostPlatform() == HOST_PLATFORM_WINDOWS) {
        /* the script travels base64-encoded, which leaves room for about 1000 characters */
        if (!host_append_say_command_windows(command, sizeof(command), text)) {
            printf("Text is too long for a Windows host (limit about 1000 characters)\n");
            return HOST_RETURN_ERROR;
        }
    } else if (!host_append_say_command(command, sizeof(command), text)) {
        printf("Text is too long\n");
        return HOST_RETURN_ERROR;
    }

    /* speech can run much longer than the idle limit for silent commands */
    if (host_print_command_output_ex(command, 1) != 0) {
        printf("Failed to speak on the host\n");
        return HOST_RETURN_ERROR;
    }

    return 0;
}
