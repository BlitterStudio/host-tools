/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
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
    require_contains(command, "xdg-mime query default text/plain");
    require_contains(command, "gtk-launch \"$desktop_file\" \"$target_uri\"");
    require_contains(command, "xdg-open \"$target\"");
    require_not_contains(command, "query filetype");
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
    require_contains(command, "xdg-mime query default text/plain");
    require_not_contains(command, "query filetype");
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

static int link_tool(const char *dir, const char *name)
{
    static const char *const places[] = { "/bin/", "/usr/bin/" };
    char source[128];
    char target[512];

    for (int i = 0; i < 2; i++) {
        snprintf(source, sizeof(source), "%s%s", places[i], name);
        snprintf(target, sizeof(target), "%s/%s", dir, name);
        if (access(source, X_OK) == 0 && symlink(source, target) == 0) {
            return 1;
        }
    }
    return 0;
}

/* run the edit script with only the tools in dir on PATH; returns the exit status */
static int run_edit_script(const char *dir, const char *editor, const char *target_path,
                           double *elapsed)
{
    char command[HOST_MAX_COMMAND_LEN];
    char wrapped[HOST_MAX_COMMAND_LEN * 2];
    struct timespec start;
    struct timespec end;
    FILE *pipe;
    char output[256];
    size_t len;
    int status;

    command[0] = '\0';
    require(host_append_edit_command(command, sizeof(command), target_path),
            "edit command should build");

    wrapped[0] = '\0';
    require(host_append_literal(wrapped, sizeof(wrapped), "/usr/bin/env -i PATH=") &&
            host_append_shell_arg(wrapped, sizeof(wrapped), dir, 0) &&
            host_append_literal(wrapped, sizeof(wrapped), editor[0] ? " VISUAL=" : "") &&
            (editor[0] == '\0' || host_append_shell_arg(wrapped, sizeof(wrapped), editor, 0)) &&
            host_append_literal(wrapped, sizeof(wrapped), " /bin/sh -c ") &&
            host_append_shell_arg(wrapped, sizeof(wrapped), command, 0),
            "wrapped edit command should fit");

    clock_gettime(CLOCK_MONOTONIC, &start);
    pipe = popen(wrapped, "r");
    require(pipe != NULL, "edit script should start");
    len = fread(output, 1, sizeof(output), pipe);
    status = pclose(pipe);
    clock_gettime(CLOCK_MONOTONIC, &end);
    require(len == 0, "the edit script must not print anything");
    *elapsed = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void test_editor_fallback_behavior(void)
{
    char dir[] = "/tmp/host-tools-edit.XXXXXX";
    char path[512];
    char editor[600];
    char contents[256];
    FILE *f;
    double elapsed;
    int found = 0;

    require(mkdtemp(dir) != NULL, "temp dir should be created");
    require(link_tool(dir, "sh") && link_tool(dir, "sleep"), "test tools should be linked");

    snprintf(path, sizeof(path), "%s/editor", dir);
    f = fopen(path, "w");
    require(f != NULL, "fake editor should be created");
    fprintf(f, "#!/bin/sh\nprintf '%%s|%%s\\n' \"$1\" \"$2\" > \"%s/args\"\nsleep 5\n", dir);
    fclose(f);
    chmod(path, 0755);

    /* with no desktop tools the VISUAL editor starts detached and the script returns */
    snprintf(editor, sizeof(editor), "%s/editor -w", dir);
    require(run_edit_script(dir, editor, "/tmp/some file.txt", &elapsed) == 0,
            "starting the editor should succeed");
    require(elapsed < 3.0, "the script must not wait for the editor to quit");
    snprintf(path, sizeof(path), "%s/args", dir);
    for (int i = 0; i < 50 && !found; i++) {
        struct timespec pause = { 0, 100000000L };

        f = fopen(path, "r");
        if (f != NULL) {
            size_t len = fread(contents, 1, sizeof(contents) - 1, f);

            fclose(f);
            contents[len] = '\0';
            found = len > 0 && contents[len - 1] == '\n';
        }
        if (!found) {
            nanosleep(&pause, NULL);
        }
    }
    require(found, "the editor should have been started");
    require(strcmp(contents, "-w|/tmp/some file.txt\n") == 0,
            "the editor should get its options and the file");

    /* a missing editor is a failure the guest can see (it used to be invisible) */
    require(run_edit_script(dir, "no-such-editor-xyz", "/tmp/a.txt", &elapsed) == 127,
            "a missing editor should exit 127");
    require(run_edit_script(dir, "", "/tmp/a.txt", &elapsed) == 127,
            "no editor at all should exit 127");
}

int main(void)
{
    test_file_uri_encoding();
    test_absolute_edit_command();
    test_relative_edit_command();
    test_small_buffer_failure();
    test_uri_retry_when_tail_would_not_fit();
    test_shell_syntax();
    test_editor_fallback_behavior();
    return 0;
}
