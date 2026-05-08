/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_common.h"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static void require_string(const char *actual, const char *expected, const char *message)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s\nexpected: %s\nactual:   %s\n", message, expected, actual);
        exit(1);
    }
}

static void test_append_literal(void)
{
    char value[8];

    value[0] = '\0';
    require(host_append_literal(value, sizeof(value), "abc"), "literal append should fit");
    require(host_append_literal(value, sizeof(value), "def"), "second literal append should fit");
    require_string(value, "abcdef", "literal appends should concatenate");

    require(!host_append_literal(value, sizeof(value), "gh"), "oversized append should fail");
    require_string(value, "abcdef", "failed append should leave buffer unchanged");
}

static void test_join_args(void)
{
    char value[32];
    char *argv[] = {"host-notify", "Title", "hello", "world"};

    value[0] = '\0';
    require(host_join_args(value, sizeof(value), 4, argv, 2), "joined args should fit");
    require_string(value, "hello world", "joined args should use single spaces");
}

static void test_shell_arg_quoting(void)
{
    char command[128];
    int needs_quote;

    command[0] = '\0';
    require(host_append_shell_arg(command, sizeof(command), "safe-Path_1:/a+b=c,d@x", 0),
            "safe shell arg should append");
    require(host_append_shell_arg(command, sizeof(command), "two words", 1),
            "spaced shell arg should append");
    require(host_append_shell_arg(command, sizeof(command), "it's ok", 1),
            "single quote shell arg should append");
    require(host_append_shell_arg(command, sizeof(command), "", 1),
            "empty shell arg should append");
    require_string(command,
                   "safe-Path_1:/a+b=c,d@x 'two words' 'it'\\''s ok' ''",
                   "shell args should be safely quoted");

    require(host_shell_arg_len("plain", &needs_quote) == 5, "plain arg length should match");
    require(!needs_quote, "plain arg should not need quoting");
    require(host_shell_arg_len("two words", &needs_quote) == 11, "quoted arg length should include quotes");
    require(needs_quote, "spaced arg should need quoting");
}

static void test_shell_arg_buffer_failure(void)
{
    char command[8] = "prefix";

    require(!host_append_shell_arg(command, sizeof(command), "toolong", 1),
            "oversized shell arg should fail");
    require_string(command, "prefix", "failed shell arg append should leave buffer unchanged");
}

static void test_uri_detection(void)
{
    require(host_is_uri("https://example.com"), "https URL should be treated as URI");
    require(host_is_uri("MAILTO:user@example.com"), "URI scheme check should be case-insensitive");
    require(host_is_uri("foo://bar"), "generic scheme with authority should be treated as URI");
    require(!host_is_uri("Work:Docs/file.txt"), "Amiga volume path should not be treated as URI");
    require(!host_is_uri("C:Tools/host-run"), "Amiga command path should not be treated as URI");
}

int main(void)
{
    test_append_literal();
    test_join_args();
    test_shell_arg_quoting();
    test_shell_arg_buffer_failure();
    test_uri_detection();
    return 0;
}
