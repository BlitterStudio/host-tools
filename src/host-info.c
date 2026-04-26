/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_capture.h"

static const char version[] = "$VER: Host-Info v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-Info v%s\n", VERSION_STR);
    printf("Host-Info prints basic host integration details.\n");
    printf("%s\nUsage: host-info\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    static const char command[] =
        "printf 'OS: '; uname -srm 2>/dev/null || printf 'unknown\\n'; "
        "printf 'User: '; id -un 2>/dev/null || whoami 2>/dev/null || printf 'unknown\\n'; "
        "printf 'Shell: %s\\n' \"${SHELL:-unknown}\"; "
        "printf 'Editor: %s\\n' \"${VISUAL:-${EDITOR:-unconfigured}}\"; "
        "printf 'Opener: '; "
        "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then echo open; "
        "elif command -v xdg-open >/dev/null 2>&1; then echo xdg-open; else echo unavailable; fi; "
        "printf 'Clipboard: '; "
        "if command -v pbcopy >/dev/null 2>&1 && command -v pbpaste >/dev/null 2>&1; then echo pbcopy/pbpaste; "
        "elif command -v wl-copy >/dev/null 2>&1 && command -v wl-paste >/dev/null 2>&1; then echo wl-clipboard; "
        "elif command -v xclip >/dev/null 2>&1; then echo xclip; "
        "elif command -v xsel >/dev/null 2>&1; then echo xsel; else echo unavailable; fi";

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
        return print_usage();
    }

    return host_print_command_output(command);
}
