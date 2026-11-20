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

static void require_not_contains(const char *value, const char *needle)
{
    if (strstr(value, needle) != NULL) {
        fprintf(stderr, "unexpected substring: %s\n", needle);
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

    require_contains(command, "curl --globoff --proto '=http,https,ftp,ftps' --proto-redir '=http,https,ftp,ftps'");
    require_contains(command, "-sIL 'https://example.com/file'\\''s.lha' 2>/dev/null | tr -d '\\r' | awk '");
    require_contains(command, "-sfL 'https://example.com/file'\\''s.lha'");
    require_contains(command, "echo 0; wget -q -O - 'https://example.com/file'\\''s.lha'");
    require_contains(command, "else exit 127; fi");
    require_not_contains(command, "%{content_length}");
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
    require_contains(command, "curl --globoff --proto '=http,https,ftp,ftps' --proto-redir '=http,https,ftp,ftps'");
    require_contains(command, "-sfL -o \"$t\" 'https://example.com/a b.adf' & p=$!");
    require_contains(command, "wget -q -T 60 -O \"$t\" 'https://example.com/a b.adf' & p=$!");
    require_contains(command, "while kill -0 \"$p\" 2>/dev/null; do printf ' '; sleep 1; done");
    require_contains(command, "wait \"$p\" || exit 22");
    require_contains(command, "wc -c < \"$t\"; base64 < \"$t\"");
    require_shell_syntax(command);
}

/* Decode the script inside "powershell -NoProfile -EncodedCommand <base64>" */
static const char *decode_ps_script(const char *command)
{
    static const char prefix[] = "powershell -NoProfile -EncodedCommand ";
    static unsigned char decoded[HOST_MAX_COMMAND_LEN * 3];
    static char script[HOST_MAX_COMMAND_LEN * 2];
    struct host_base64_state st;
    long n;

    require(strncmp(command, prefix, sizeof(prefix) - 1) == 0,
            "windows download command should be an encoded powershell command");
    host_base64_init(&st);
    n = host_base64_feed(&st, command + sizeof(prefix) - 1,
                         (long)strlen(command + sizeof(prefix) - 1), decoded);
    require(n > 0 && n % 2 == 0, "decoded script should be UTF-16");
    for (long i = 0; i < n / 2; i++) {
        require(decoded[i * 2 + 1] == 0, "script should be Latin-1 text");
        script[i] = (char)decoded[i * 2];
    }
    script[n / 2] = '\0';
    return script;
}

static void test_windows_stream_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    const char *script;

    command[0] = '\0';
    require(host_append_download_stream_command_windows(command, sizeof(command),
                                                        "https://example.com/a'b/%TEMP%.lha?x=%20&y=[1-2]{a,b}"),
            "windows stream download command should build");

    /* nothing but the powershell launcher and base64 reaches cmd.exe, so
     * %NAME%, &, quotes and ^ in the URL cannot be interpreted by it */
    for (const char *p = command; *p; p++) {
        require((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') || *p == ' ' || *p == '-' ||
                *p == '+' || *p == '/' || *p == '=',
                "windows download command must not contain cmd metacharacters");
    }

    script = decode_ps_script(command);
    require_contains(script, "$u='https://example.com/a''b/%TEMP%.lha?x=%20&y=[1-2]{a,b}'");
    require_contains(script, "& curl.exe --globoff --proto $p --proto-redir $p --max-time 15 -sIL $u");
    require_contains(script, "& curl.exe --globoff --proto $p --proto-redir $p -sfL $u;exit $LASTEXITCODE");
    require_not_contains(script, "content_length");

    command[0] = '\0';
    require(!host_append_download_stream_command_windows(command, sizeof(command),
                                                         "https://example.com/a\"b"),
            "embedded quotes cannot be passed to curl.exe");
    require(!host_download_url_windows_safe("https://example.com/a\nb"),
            "line breaks cannot be passed to curl.exe");
    require(host_download_url_windows_safe("https://example.com/a b%20c"),
            "spaces and percent signs are fine");
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

    command[0] = '\0';
    require(!host_append_download_stream_command_windows(command, sizeof(command),
                                                         "https://e.com/f"),
            "small windows command buffer should fail");
}

static void require_filename(const char *url, const char *expected)
{
    char name[108];

    require(host_url_filename(url, name, sizeof(name)) == HOST_URL_FILENAME_OK,
            "filename should be accepted");
    if (strcmp(name, expected) != 0) {
        fprintf(stderr, "filename for %s: expected %s, got %s\n", url, expected, name);
        exit(1);
    }
}

static void require_invalid_filename(const char *url)
{
    char name[108];

    if (host_url_filename(url, name, sizeof(name)) != HOST_URL_FILENAME_INVALID) {
        fprintf(stderr, "filename for %s should be rejected, got %s\n", url, name);
        exit(1);
    }
    require(name[0] == '\0', "rejected filename should be empty");
}

static void test_url_filename(void)
{
    require_filename("https://example.com/path/file.lha", "file.lha");
    require_filename("https://example.com/file.lha?v=1#frag", "file.lha");
    require_filename("https://example.com/path/", "download");
    require_filename("https://example.com/", "download");
    require_filename("https://example.com", "download");
    require_filename("https://user:pw@example.com:8080", "download");
    require_filename("https://example.com/a?next=OUT:x/y", "a");
    require_filename("ftp://example.com/a/b/c.adf", "c.adf");
    require_filename("https://example.com/a%3Ab", "a%3Ab");

    {
        char name[8];
        require(host_url_filename("https://example.com/verylongname.bin", name, sizeof(name)) ==
                    HOST_URL_FILENAME_TRUNCATED,
                "long filenames should report truncation");
        require(strcmp(name, "verylon") == 0, "long filenames should truncate");
        require(host_url_filename("https://example.com/1234567", name, sizeof(name)) ==
                    HOST_URL_FILENAME_OK,
                "a name that exactly fits is not truncated");
    }
}

/* AmigaDOS would read OUT:payload.bin as a path on another volume. */
static void test_url_filename_must_be_one_component(void)
{
    require_invalid_filename("https://example.com/OUT:payload.bin");
    require_invalid_filename("https://example.com/dir/SYS:Dir");
    require_invalid_filename("https://example.com/:x");
    require_invalid_filename("https://example.com/..");
    require_invalid_filename("https://example.com/a/.");
    require_invalid_filename("https://example.com/a\nb");
    require_invalid_filename("https://example.com/a\x01" "b");
}

static void test_supported_urls(void)
{
    require(host_download_url_supported("https://example.com/file.lha"),
            "HTTPS should be supported");
    require(host_download_url_supported("HTTP://example.com/file.lha"),
            "URL scheme matching should be case-insensitive");
    require(host_download_url_supported("ftp://example.com/file.lha"),
            "FTP should be supported");
    require(host_download_url_supported("ftps://example.com/file.lha"),
            "FTPS should be supported");
    require(!host_download_url_supported("file:///etc/passwd"),
            "local file URLs should be rejected");
    require(!host_download_url_supported("data:text/plain,hello"),
            "data URLs should be rejected");
    require(!host_download_url_supported("ssh://example.com/file"),
            "arbitrary URL schemes should be rejected");
}

static void test_sidecar_path(void)
{
    char path[128];
    const char *component;

    require(host_download_sidecar_path("RAM:Host-Tools-2.6.lha", 'p', 0x12, 3,
                                       path, sizeof(path)),
            "sidecar path should build");
    require(strcmp(path, "RAM:.htp00001203") == 0,
            "sidecar path should use a short sibling filename");
    component = strrchr(path, ':');
    require(component != NULL && strlen(component + 1) == 12,
            "sidecar component should fit classic FFS limits");

    require(host_download_sidecar_path("Work:Downloads/very-long-destination-name.lha",
                                       'b', 0xabcdef, 99, path, sizeof(path)),
            "nested backup sidecar path should build");
    require(strcmp(path, "Work:Downloads/.htbabcdef99") == 0,
            "backup sidecar should preserve only the destination directory");

    require(host_download_sidecar_path("relative-name-with-thirty-chars.bin",
                                       'p', 1, 0, path, sizeof(path)),
            "relative sidecar path should build");
    require(strcmp(path, ".htp00000100") == 0,
            "relative sidecar should remain in the current directory");

    require(!host_download_sidecar_path("RAM:archive.lha", 'p', 0x12, 3,
                                        path, 16),
            "small sidecar path buffer should fail");
    require(!host_download_sidecar_path("RAM:archive.lha", 'x', 0x12, 3,
                                        path, sizeof(path)),
            "unknown sidecar kind should fail");
    require(!host_download_sidecar_path("RAM:archive.lha", 'p', 0x12, 100,
                                        path, sizeof(path)),
            "sidecar attempt outside the two-digit range should fail");
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

static void test_base64_encode_utf16le(void)
{
    char out[64];

    require(host_base64_encode_utf16le("ab", out, sizeof(out)),
            "utf16le encoding should build");
    require(strcmp(out, "YQBiAA==") == 0, "utf16le base64 should match");

    require(host_base64_encode_utf16le("", out, sizeof(out)),
            "empty utf16le encoding should build");
    require(out[0] == '\0', "empty input should encode to an empty string");

    require(!host_base64_encode_utf16le("hello world", out, 8),
            "small encode buffer should fail");
}

int main(void)
{
    test_stream_command();
    test_b64_command();
    test_windows_stream_command();
    test_small_buffer_failures();
    test_url_filename();
    test_url_filename_must_be_one_component();
    test_supported_urls();
    test_sidecar_path();
    test_base64_decode();
    test_base64_encode_utf16le();
    return 0;
}
