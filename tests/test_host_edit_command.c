/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_edit_command.h"

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

static void require_not_contains(const char *value, const char *needle)
{
    if (strstr(value, needle) != NULL) {
        fprintf(stderr, "unexpected substring: %s\n", needle);
        fprintf(stderr, "value: %s\n", value);
        exit(1);
    }
}

static void test_file_uri_encoding(void)
{
    char uri[HOST_FILE_URI_MAX_LEN];

    require(host_path_to_file_uri("/tmp/a b#c%'file.txt", uri, sizeof(uri)),
            "absolute path should convert to file URI");
    require(strcmp(uri, "file:///tmp/a%20b%23c%25%27file.txt") == 0,
            "file URI should percent-encode unsafe bytes");
    require(!host_path_to_file_uri("relative.txt", uri, sizeof(uri)),
            "relative path should not convert to file URI");
}

static void test_absolute_edit_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_edit_command(command, sizeof(command), "/tmp/a b.txt"),
            "absolute edit command should build");

    require_contains(command, "target='/tmp/a b.txt'; target_uri='file:///tmp/a%20b.txt'");
    require_contains(command, "gtk-launch \"$desktop_file\" \"$target_uri\"");
    require_contains(command, "xdg-open \"$target\"");
    require_contains(command, "sh -c 'exec $HOST_EDITOR \"$1\"' sh \"$target\"");
    require_not_contains(command, "xdg-open '/tmp/a b.txt'");
}

static void test_relative_edit_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_edit_command(command, sizeof(command), "newfile.txt"),
            "relative edit command should build");

    require_contains(command, "target=newfile.txt; target_uri=;");
    require_contains(command, "xdg-mime query filetype \"$target\"");
    require_contains(command, "xdg-open \"$target\"");
}

static void test_small_buffer_failure(void)
{
    char command[16];

    command[0] = '\0';
    require(!host_append_edit_command(command, sizeof(command), "/tmp/a.txt"),
            "too-small command buffer should fail");
}

static void test_uri_retry_when_tail_would_not_fit(void)
{
    static const char path[] = "/tmp/a b.txt";
    char command[HOST_MAX_COMMAND_LEN];
    char uri[HOST_FILE_URI_MAX_LEN];
    char *fallback_command;
    size_t fallback_size;
    int needs_quote;

    command[0] = '\0';
    require(host_path_to_file_uri(path, uri, sizeof(uri)),
            "test path should convert to file URI");
    require(host_append_edit_command(command, sizeof(command), path),
            "full edit command should build");

    fallback_size = strlen(command) - host_shell_arg_len(uri, &needs_quote) + 1;
    fallback_command = malloc(fallback_size);
    require(fallback_command != NULL, "fallback command buffer should allocate");

    fallback_command[0] = '\0';
    require(host_append_edit_command(fallback_command, fallback_size, path),
            "command should retry without URI when full command does not fit");
    require_contains(fallback_command, "target='/tmp/a b.txt'; target_uri=;");
    require_not_contains(fallback_command, "file:///tmp/a%20b.txt");

    free(fallback_command);
}

static void require_shell_syntax(const char *path)
{
    char command[HOST_MAX_COMMAND_LEN];
    char syntax_command[HOST_MAX_COMMAND_LEN * 2];

    command[0] = '\0';
    syntax_command[0] = '\0';
    require(host_append_edit_command(command, sizeof(command), path),
            "edit command should build for shell syntax check");
    require(host_append_literal(syntax_command, sizeof(syntax_command), "sh -n -c "),
            "syntax command prefix should fit");
    require(host_append_shell_arg(syntax_command, sizeof(syntax_command), command, 0),
            "syntax command should fit");
    require(system(syntax_command) == 0, "generated shell command should parse");
}

static void test_shell_syntax(void)
{
    require_shell_syntax("/tmp/a b's.txt");
    require_shell_syntax("newfile.txt");
}

int main(void)
{
    test_file_uri_encoding();
    test_absolute_edit_command();
    test_relative_edit_command();
    test_small_buffer_failure();
    test_uri_retry_when_tail_would_not_fit();
    test_shell_syntax();
    return 0;
}
