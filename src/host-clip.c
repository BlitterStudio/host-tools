/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"

static const char version[] = "$VER: Host-Clip v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Clip v%s\n", VERSION_STR);
    printf("Host-Clip copies text to or pastes text from the host clipboard.\n");
    printf("%s\nUsage: host-clip [copy] <text...>\n       host-clip paste\n", version);
    return 0;
}

static int append_copy_command(char *command, size_t command_size, const char *text)
{
    return host_append_literal(command, command_size, "printf %s ") &&
           host_append_shell_arg(command, command_size, text, 0) &&
           host_append_literal(command, command_size,
                               " | if command -v pbcopy >/dev/null 2>&1; then pbcopy; elif command -v wl-copy >/dev/null 2>&1; then wl-copy; elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard; elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --input; else printf 'No host clipboard backend found\\n' >&2; exit 127; fi");
}

int main(int argc, char *argv[])
{
    char command[HOST_MAX_COMMAND_LEN] = "";
    char text[2048] = "";
    int start = 1;

    static const char paste_command[] =
        "if command -v pbpaste >/dev/null 2>&1; then pbpaste; "
        "elif command -v wl-paste >/dev/null 2>&1; then wl-paste -n; "
        "elif command -v xclip >/dev/null 2>&1; then xclip -selection clipboard -o; "
        "elif command -v xsel >/dev/null 2>&1; then xsel --clipboard --output; "
        "else printf 'No host clipboard backend found\\n' >&2; exit 127; fi";

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing clipboard argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (strcmp(argv[1], "paste") == 0 || strcmp(argv[1], "-p") == 0) {
        if (argc != 2) {
            printf("Unexpected argument after paste\n");
            return print_usage();
        }
        return host_print_command_output(paste_command);
    }

    if (strcmp(argv[1], "copy") == 0 || strcmp(argv[1], "-c") == 0) {
        start = 2;
    }

    if (argc <= start) {
        printf("Missing text argument\n");
        return print_usage();
    }

    if (!host_join_args(text, sizeof(text), argc, argv, start)) {
        printf("Clipboard text is too long\n");
        return HOST_RETURN_ERROR;
    }

    if (!append_copy_command(command, sizeof(command), text)) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    return host_print_command_output(command);
}
