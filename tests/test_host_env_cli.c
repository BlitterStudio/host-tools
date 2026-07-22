/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_PLATFORM_POSIX 1
#define HOST_PLATFORM_WINDOWS 3

static const char *captured_command;

static int InitUAEResource(void)
{
    return 1;
}

static int GetHostPlatform(void)
{
    return HOST_PLATFORM_POSIX;
}

static int host_print_command_output(const char *command)
{
    captured_command = command;
    return 0;
}

#define HOST_CAPTURE_H
#define main host_env_main
#include "../src/host-env.c"
#undef main

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static void test_get_dispatch(void)
{
    char *argv[] = { "host-env", "get", "HOST_TOOLS_TEST", NULL };

    captured_command = NULL;
    require(host_env_main(3, argv) == 0, "get should dispatch successfully");
    require(captured_command != NULL, "get should execute a host command");
    require(strstr(captured_command, "HOST_TOOLS_TEST") != NULL,
            "get command should contain the requested variable name");
}

static void test_set_dispatch(void)
{
    char *argv[] = { "host-env", "set", "HOST_TOOLS_TEST", "value with spaces", NULL };

    captured_command = NULL;
    require(host_env_main(4, argv) == 0, "set should dispatch successfully");
    require(captured_command != NULL, "set should execute a host command");
    require(strstr(captured_command, "value with spaces") != NULL,
            "set command should contain the requested value");
}

int main(void)
{
    test_get_dispatch();
    test_set_dispatch();
    return 0;
}
