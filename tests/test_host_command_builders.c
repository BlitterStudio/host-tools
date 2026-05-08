/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_clip_command.h"
#include "host_info_command.h"
#include "host_notify_command.h"
#include "host_reveal_command.h"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static void require_contains(const char *value, const char *needle)
{
    if (strstr(value, needle) == NULL) {
        fprintf(stderr, "missing substring: %s\n", needle);
        fprintf(stderr, "value: %s\n", value);
        exit(1);
    }
}

static void require_shell_syntax(const char *command)
{
    char syntax_command[HOST_MAX_COMMAND_LEN * 2];

    syntax_command[0] = '\0';
    require(host_append_literal(syntax_command, sizeof(syntax_command), "sh -n -c "),
            "syntax command prefix should fit");
    require(host_append_shell_arg(syntax_command, sizeof(syntax_command), command, 0),
            "syntax command should fit");
    require(system(syntax_command) == 0, "generated shell command should parse");
}

static void test_reveal_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_reveal_command(command, sizeof(command), "/tmp/a b's.txt"),
            "reveal command should build");

    require_contains(command, "open -R '/tmp/a b'\\''s.txt'");
    require_contains(command, "xdg-open \"$(dirname -- '/tmp/a b'\\''s.txt')\"");
    require_contains(command, "else exit 127; fi");
    require_shell_syntax(command);
}

static void test_notify_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_notify_command(command, sizeof(command),
                                       "Build's Done", "Hello $USER & goodbye"),
            "notify command should build");

    require_contains(command, "notify-send 'Build'\\''s Done' 'Hello $USER & goodbye'");
    require_contains(command, "osascript -e 'on run argv'");
    require_contains(command, "'Build'\\''s Done' 'Hello $USER & goodbye'; else exit 127; fi");
    require_shell_syntax(command);
}

static void test_clip_copy_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_clip_copy_command(command, sizeof(command),
                                          "copy $HOME and 'quotes'"),
            "clipboard copy command should build");

    require_contains(command, "printf %s 'copy $HOME and '\\''quotes'\\''' |");
    require_contains(command, "pbcopy");
    require_contains(command, "wl-copy");
    require_contains(command, "xclip -selection clipboard");
    require_contains(command, "xsel --clipboard --input");
    require_shell_syntax(command);
}

static void test_clip_paste_command(void)
{
    require_contains(HOST_CLIP_PASTE_COMMAND, "pbpaste");
    require_contains(HOST_CLIP_PASTE_COMMAND, "wl-paste -n");
    require_contains(HOST_CLIP_PASTE_COMMAND, "xclip -selection clipboard -o");
    require_contains(HOST_CLIP_PASTE_COMMAND, "xsel --clipboard --output");
    require_shell_syntax(HOST_CLIP_PASTE_COMMAND);
}

static void test_info_command(void)
{
    require_contains(HOST_INFO_COMMAND, "printf 'OS: '");
    require_contains(HOST_INFO_COMMAND, "printf 'Editor: '");
    require_contains(HOST_INFO_COMMAND, "xdg-mime query default text/plain");
    require_contains(HOST_INFO_COMMAND, "printf 'Opener: '");
    require_contains(HOST_INFO_COMMAND, "printf 'Clipboard: '");
    require_shell_syntax(HOST_INFO_COMMAND);
}

static void test_small_buffer_failures(void)
{
    char command[32];

    command[0] = '\0';
    require(!host_append_reveal_command(command, sizeof(command), "/tmp/a.txt"),
            "small reveal command buffer should fail");

    command[0] = '\0';
    require(!host_append_notify_command(command, sizeof(command), "Title", "Message"),
            "small notify command buffer should fail");

    command[0] = '\0';
    require(!host_append_clip_copy_command(command, sizeof(command), "Text"),
            "small clipboard copy command buffer should fail");
}

int main(void)
{
    test_reveal_command();
    test_notify_command();
    test_clip_copy_command();
    test_clip_paste_command();
    test_info_command();
    test_small_buffer_failures();
    return 0;
}
