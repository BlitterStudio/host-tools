/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_base64.h"
#include "host_download_command.h"

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

static void test_stream_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_download_stream_command(command, sizeof(command),
                                                "https://example.com/file's.lha"),
            "stream download command should build");

    require_contains(command, "curl -sfIL -o /dev/null -w '%{content_length}\\n'");
    require_contains(command, "curl -sfL 'https://example.com/file'\\''s.lha'");
    require_contains(command, "echo 0; wget -q -O - 'https://example.com/file'\\''s.lha'");
    require_contains(command, "else exit 127; fi");
    require_shell_syntax(command);
}

static void test_b64_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_download_b64_command(command, sizeof(command),
                                             "https://example.com/a b.adf"),
            "base64 download command should build");

    require_contains(command, "t=$(mktemp) || exit 1");
    require_contains(command, "curl -sfL -o \"$t\" 'https://example.com/a b.adf'");
    require_contains(command, "wget -q -O \"$t\" 'https://example.com/a b.adf'");
    require_contains(command, "|| { rm -f \"$t\"; exit 22; }");
    require_contains(command, "wc -c < \"$t\"; base64 < \"$t\"; rm -f \"$t\"");
    require_shell_syntax(command);
}

static void test_windows_stream_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_download_stream_command_windows(command, sizeof(command),
                                                        "https://example.com/file.lha"),
            "windows stream download command should build");

    require_contains(command,
                     "(curl -sIL -o NUL -w \"%{content_length}\\n\" \"https://example.com/file.lha\" || echo 0)");
    require_contains(command, "& curl -sfL \"https://example.com/file.lha\"");

    command[0] = '\0';
    require(!host_append_download_stream_command_windows(command, sizeof(command),
                                                         "https://example.com/a\"b"),
            "embedded quotes should be rejected for cmd quoting");
}

static void test_small_buffer_failures(void)
{
    char command[32];

    command[0] = '\0';
    require(!host_append_download_stream_command(command, sizeof(command), "https://e.com/f"),
            "small stream command buffer should fail");

    command[0] = '\0';
    require(!host_append_download_b64_command(command, sizeof(command), "https://e.com/f"),
            "small base64 command buffer should fail");
}

static void require_filename(const char *url, const char *expected)
{
    char name[108];

    host_url_filename(url, name, sizeof(name));
    if (strcmp(name, expected) != 0) {
        fprintf(stderr, "filename for %s: expected %s, got %s\n", url, expected, name);
        exit(1);
    }
}

static void test_url_filename(void)
{
    require_filename("https://example.com/path/file.lha", "file.lha");
    require_filename("https://example.com/file.lha?v=1#frag", "file.lha");
    require_filename("https://example.com/path/", "download");
    require_filename("https://example.com/", "download");
    require_filename("https://example.com", "download");
    require_filename("ftp://example.com/a/b/c.adf", "c.adf");

    {
        char name[8];
        host_url_filename("https://example.com/verylongname.bin", name, sizeof(name));
        require(strcmp(name, "verylon") == 0, "long filenames should truncate");
    }
}

static void require_decoded(const char *encoded, const char *expected, long expected_len)
{
    struct host_base64_state st;
    unsigned char out[64];
    long n;

    host_base64_init(&st);
    n = host_base64_feed(&st, encoded, (long)strlen(encoded), out);
    require(n == expected_len, "decoded length should match");
    require(memcmp(out, expected, (size_t)expected_len) == 0, "decoded bytes should match");
    require(st.nbits < 6, "no leftover base64 bits expected");
}

static void test_base64_decode(void)
{
    struct host_base64_state st;
    unsigned char out[64];
    long n;

    require_decoded("aGVsbG8=", "hello", 5);
    require_decoded("aGVs\r\nbG8=\r\n", "hello", 5);
    require_decoded("  aGVsbG8gd29ybGQ=  ", "hello world", 11);
    require_decoded("AAEC/w==", "\x00\x01\x02\xff", 4);
    require_decoded("", "", 0);

    /* split feeding keeps state across chunks */
    host_base64_init(&st);
    n = host_base64_feed(&st, "aGV", 3, out);
    require(n == 2, "first chunk should decode two bytes");
    n += host_base64_feed(&st, "sbG8=", 5, out + n);
    require(n == 5, "split feed should decode five bytes");
    require(memcmp(out, "hello", 5) == 0, "split feed bytes should match");

    /* invalid input */
    host_base64_init(&st);
    require(host_base64_feed(&st, "aG!s", 4, out) == -1,
            "invalid characters should fail");

    /* data after padding */
    host_base64_init(&st);
    require(host_base64_feed(&st, "aGVsbG8=X", 9, out) == -1,
            "data after padding should fail");
}

int main(void)
{
    test_stream_command();
    test_b64_command();
    test_windows_stream_command();
    test_small_buffer_failures();
    test_url_filename();
    test_base64_decode();
    return 0;
}
