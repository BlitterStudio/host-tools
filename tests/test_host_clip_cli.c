/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#define main host_clip_main
#include "../src/host-clip.c"
#undef main

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static FILE *file_with(const char *bytes, size_t length)
{
    FILE *f = tmpfile();

    require(f != NULL, "temporary file should open");
    require(fwrite(bytes, 1, length, f) == length, "temporary file should be written");
    rewind(f);
    return f;
}

static void test_read_text_results(void)
{
    char text[16];
    FILE *f;

    f = file_with("hello\n", 6);
    require(host_clip_read_text(f, text, sizeof(text)) == HOST_CLIP_INPUT_OK, "plain text should be accepted");
    require(strcmp(text, "hello\n") == 0, "text should be kept verbatim");
    fclose(f);

    f = file_with("0123456789abcde", 15);
    require(host_clip_read_text(f, text, sizeof(text)) == HOST_CLIP_INPUT_OK,
            "text that exactly fills the buffer should be accepted");
    fclose(f);

    f = file_with("0123456789abcdef", 16);
    require(host_clip_read_text(f, text, sizeof(text)) == HOST_CLIP_INPUT_TOO_LONG,
            "text one byte too long should be rejected as too long");
    fclose(f);

    f = file_with("before\0after", 12);
    require(host_clip_read_text(f, text, sizeof(text)) == HOST_CLIP_INPUT_HAS_NUL,
            "embedded NUL should be rejected");
    fclose(f);

    f = file_with("\0", 1);
    require(host_clip_read_text(f, text, sizeof(text)) == HOST_CLIP_INPUT_HAS_NUL,
            "a lone NUL should be rejected");
    fclose(f);

    /* a stream that cannot be read reports an error instead of empty text */
    f = tmpfile();
    require(f != NULL, "temporary file should open");
    {
        FILE *write_only = fdopen(dup(fileno(f)), "w");

        require(write_only != NULL, "write-only stream should open");
        require(host_clip_read_text(write_only, text, sizeof(text)) == HOST_CLIP_INPUT_READ_ERROR,
                "a read error should be reported");
        fclose(write_only);
    }
    fclose(f);
}

static int run_copy_from(FILE *input)
{
    char *argv[] = { "host-clip", "copy", NULL };

    stdin = input;
    captured_command = NULL;
    return host_clip_main(2, argv);
}

static void test_copy_from_stdin(void)
{
    FILE *f;

    f = file_with("hello clip\n", 11);
    require(run_copy_from(f) == 0, "plain stdin text should be copied");
    require(captured_command != NULL && strstr(captured_command, "hello clip") != NULL,
            "the copy command should carry the text");
    fclose(f);

    f = file_with("before\0after", 12);
    require(run_copy_from(f) == HOST_RETURN_ERROR, "NUL in stdin should fail");
    require(captured_command == NULL, "NUL input must not reach the host");
    fclose(f);

    {
        static char big[3000];

        memset(big, 'a', sizeof(big));
        f = file_with(big, sizeof(big));
        require(run_copy_from(f) == HOST_RETURN_ERROR, "oversized stdin should fail");
        require(captured_command == NULL, "oversized input must not reach the host");
        fclose(f);
    }

    /* a read error must not clear the clipboard with empty text */
    f = tmpfile();
    require(f != NULL, "temporary file should open");
    {
        FILE *write_only = fdopen(dup(fileno(f)), "w");

        require(write_only != NULL, "write-only stream should open");
        require(run_copy_from(write_only) == HOST_RETURN_ERROR, "unreadable stdin should fail");
        require(captured_command == NULL, "a failed read must not reach the host");
        fclose(write_only);
    }
    fclose(f);
}

int main(void)
{
    test_read_text_results();
    test_copy_from_stdin();
    return 0;
}
