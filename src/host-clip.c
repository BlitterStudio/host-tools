/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"
#include "host_clip_command.h"

static const char version[] = "$VER: Host-Clip " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Clip v%s\n", VERSION_STR);
    printf("Host-Clip copies text to or pastes text from the host clipboard.\n");
    printf("%s\nUsage: host-clip [copy] <text...>\n"
           "       host-clip copy < file (reads standard input)\n"
           "       host-clip paste\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char text[2048];
    int start = 1;

    command[0] = '\0';
    text[0] = '\0';

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing clipboard argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (strcmp(argv[1], "paste") == 0 || strcmp(argv[1], "-p") == 0) {
        if (argc != 2) {
            printf("Unexpected argument after paste\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (GetHostPlatform() == HOST_PLATFORM_WINDOWS) {
            if (!host_append_clip_paste_command_windows(command, sizeof(command))) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
            return host_print_command_output(command);
        }
        return host_print_command_output(HOST_CLIP_PASTE_COMMAND);
    }

    if (strcmp(argv[1], "copy") == 0 || strcmp(argv[1], "-c") == 0) {
        start = 2;
    }

    if (argc <= start) {
        if (start == 1) {
            printf("Missing text argument\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        /* "host-clip copy" without text reads standard input verbatim */
        switch (host_clip_read_text(stdin, text, sizeof(text))) {
        case HOST_CLIP_INPUT_OK:
            break;
        case HOST_CLIP_INPUT_TOO_LONG:
            printf("Clipboard text is too long\n");
            return HOST_RETURN_ERROR;
        case HOST_CLIP_INPUT_HAS_NUL:
            printf("Clipboard text must not contain NUL bytes\n");
            return HOST_RETURN_ERROR;
        default:
            printf("Cannot read the clipboard text from standard input\n");
            return HOST_RETURN_ERROR;
        }
    } else if (!host_join_args(text, sizeof(text), argc, argv, start)) {
        printf("Clipboard text is too long\n");
        return HOST_RETURN_ERROR;
    }

    if (GetHostPlatform() == HOST_PLATFORM_WINDOWS) {
        if (!host_append_clip_copy_command_windows(command, sizeof(command), text)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
    } else if (!host_append_clip_copy_command(command, sizeof(command), text)) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    return host_print_command_output(command);
}
