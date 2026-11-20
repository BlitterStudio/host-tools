/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "host_base64.h"
#include "host_copy_command.h"

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

static void require_string(const char *actual, const char *expected, const char *message)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s\nexpected: %s\nactual:   %s\n", message, expected, actual);
        exit(1);
    }
}

/* ---- helpers for running generated POSIX commands ---- */

static char workdir[256];

static void make_workdir(void)
{
    snprintf(workdir, sizeof(workdir), "/tmp/host-copy-test-XXXXXX");
    require(mkdtemp(workdir) != NULL, "temporary directory should be created");
}

static void path_in(char *out, size_t size, const char *name)
{
    snprintf(out, size, "%s/%s", workdir, name);
}

static void write_file(const char *path, const unsigned char *data, size_t len)
{
    FILE *f = fopen(path, "wb");

    require(f != NULL, "test file should open for writing");
    require(fwrite(data, 1, len, f) == len, "test file should be written");
    fclose(f);
}

/* Returns the length, or -1 when the file does not exist. */
static long read_file(const char *path, unsigned char *data, size_t size)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL) {
        return -1;
    }
    n = fread(data, 1, size, f);
    fclose(f);
    return (long)n;
}

static int file_exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

/* Count the entries named .htput.* left in the work directory. */
static int leftover_temp_files(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    int count = 0;

    require(d != NULL, "work directory should open");
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, ".htput.", 7) == 0) {
            count++;
        }
    }
    closedir(d);
    return count;
}

/*
 * Run "<input> | sh -c <command> > out" the way a pipe session does and
 * return the shell's exit status. input may be NULL for no stdin data.
 */
static const char *run_cwd;
static const char *run_producer;

static int run_command(const char *command, const char *input_file, const char *out_file)
{
    static char line[HOST_MAX_COMMAND_LEN * 2];
    int status;

    line[0] = '\0';
    if (run_cwd != NULL) {
        require(host_append_literal(line, sizeof(line), "cd ") &&
                host_append_shell_arg(line, sizeof(line), run_cwd, 0) &&
                host_append_literal(line, sizeof(line), " && "),
                "cd prefix should fit");
    }
    if (run_producer != NULL) {
        /* a shell fragment that runs before the data, while the host is already waiting for it */
        require(host_append_literal(line, sizeof(line), "{ ") &&
                host_append_literal(line, sizeof(line), run_producer) &&
                host_append_literal(line, sizeof(line), "; ") &&
                host_append_literal(line, sizeof(line), "cat ") &&
                host_append_shell_arg(line, sizeof(line), input_file, 0) &&
                host_append_literal(line, sizeof(line), "; } | "),
                "producer should fit");
    } else if (input_file != NULL) {
        require(host_append_literal(line, sizeof(line), "cat ") &&
                host_append_shell_arg(line, sizeof(line), input_file, 0) &&
                host_append_literal(line, sizeof(line), " | "),
                "input pipe should fit");
    } else {
        require(host_append_literal(line, sizeof(line), "true | "), "empty input should fit");
    }
    require(host_append_literal(line, sizeof(line), "sh -c ") &&
            host_append_shell_arg(line, sizeof(line), command, 0) &&
            host_append_literal(line, sizeof(line), " >") &&
            host_append_shell_arg(line, sizeof(line), out_file, 0) &&
            host_append_literal(line, sizeof(line), " 2>/dev/null"),
            "run line should fit");
    status = system(line);
    require(status != -1 && WIFEXITED(status), "command should exit normally");
    return WEXITSTATUS(status);
}

static void all_bytes(unsigned char *data)
{
    for (int i = 0; i < 256; i++) {
        data[i] = (unsigned char)i;
    }
}

/* ---- encoding and name helpers ---- */

static void test_latin1_to_utf8(void)
{
    char out[32];

    require(host_copy_latin1_to_utf8("caf\xe9 \xff", out, sizeof(out)), "conversion should succeed");
    require_string(out, "caf\xc3\xa9 \xc3\xbf", "Latin-1 bytes should become UTF-8");

    require(host_copy_latin1_to_utf8("plain.txt", out, sizeof(out)), "ASCII should convert");
    require_string(out, "plain.txt", "ASCII should be unchanged");

    require(!host_copy_latin1_to_utf8("\xe9\xe9", out, 4), "two-byte expansion must respect the buffer");
    require(host_copy_latin1_to_utf8("\xe9", out, 3), "exact fit should convert");
    require(!host_copy_latin1_to_utf8("ab", out, 2), "no room for the terminator should fail");
}

static void test_text_encoding(void)
{
    char out[64];

    require(host_copy_is_utf8("caf\xc3\xa9"), "UTF-8 e acute is UTF-8");
    require(!host_copy_is_utf8("caf\xe9"), "Latin-1 e acute is not UTF-8");
    require(!host_copy_is_utf8("plain"), "ASCII has nothing to convert");
    require(!host_copy_is_utf8("\xc3"), "a lone lead byte is not UTF-8");
    require(!host_copy_is_utf8("\xc0\x80"), "an overlong form is not UTF-8");
    require(!host_copy_is_utf8("\xed\xa0\x80"), "a surrogate is not UTF-8");
    require(!host_copy_is_utf8("a\xc3\xa9\xe9"), "mixed text is Latin-1");

    require(host_copy_to_host_utf8("caf\xc3\xa9", out, sizeof(out)), "UTF-8 should pass");
    require_string(out, "caf\xc3\xa9", "UTF-8 must not be converted a second time");
    require(host_copy_to_host_utf8("caf\xe9", out, sizeof(out)), "Latin-1 should convert");
    require_string(out, "caf\xc3\xa9", "Latin-1 is converted exactly once");
    require(!host_copy_to_host_utf8("caf\xc3\xa9", out, 5), "UTF-8 passthrough must respect the buffer");

    require(host_copy_to_ps_text("caf\xc3\xa9 \xc3\xbf", out, sizeof(out)), "UTF-8 for PowerShell");
    require_string(out, "caf\xe9 \xff", "UTF-8 up to U+00FF becomes ISO-8859-1 for the script");
    require(host_copy_to_ps_text("caf\xe9", out, sizeof(out)), "Latin-1 for PowerShell");
    require_string(out, "caf\xe9", "Latin-1 is passed unchanged to the script");
    require(host_copy_to_ps_text("\xe2\x82\xac", out, sizeof(out)), "a euro sign stays as written");
    require_string(out, "\xe2\x82\xac", "UTF-8 beyond U+00FF is not decoded");
}

static void test_put_events(void)
{
    char line[64];
    const char *code = NULL;

    strcpy(line, "READY");
    require(host_copy_put_event_for(line, 0, 100, &code) == HOST_COPY_PUT_READY, "READY");
    strcpy(line, "OK 100");
    require(host_copy_put_event_for(line, 100, 100, &code) == HOST_COPY_PUT_CONFIRMED,
            "OK right after the last write confirms the copy");
    strcpy(line, "OK 100");
    require(host_copy_put_event_for(line, 4096, 100000, &code) == HOST_COPY_PUT_UNEXPECTED,
            "OK before the end of the file is unexpected");
    strcpy(line, "OK 99");
    require(host_copy_put_event_for(line, 100, 100, &code) == HOST_COPY_PUT_UNEXPECTED,
            "OK with the wrong size is unexpected");
    strcpy(line, "OK 0");
    require(host_copy_put_event_for(line, 0, 0, &code) == HOST_COPY_PUT_CONFIRMED, "empty file confirmation");
    strcpy(line, "ERR SIZE");
    require(host_copy_put_event_for(line, 10, 100, &code) == HOST_COPY_PUT_ERROR, "ERR at any point");
    require_string(code, "SIZE", "ERR code is passed on");
    strcpy(line, "nonsense");
    require(host_copy_put_event_for(line, 10, 100, &code) == HOST_COPY_PUT_UNEXPECTED, "garbage");
}

static void test_names(void)
{
    require_string(host_copy_host_basename("/tmp/a b/file.txt", 0), "file.txt", "POSIX basename");
    require_string(host_copy_host_basename("/tmp/dir/", 0), "", "trailing slash has no name");
    require_string(host_copy_host_basename("a\\b.txt", 0), "a\\b.txt", "backslash is a name character on POSIX");
    require_string(host_copy_host_basename("C:\\Users\\me\\file.txt", 1), "file.txt", "Windows basename");
    require_string(host_copy_host_basename("C:file.txt", 1), "file.txt", "Windows drive-relative basename");
    require_string(host_copy_host_basename("C:\\dir/file.txt", 1), "file.txt", "mixed separators");

    require(host_copy_amiga_name_ok("file.txt"), "plain Amiga name");
    require(host_copy_amiga_name_ok("a b'c"), "spaces and quotes are fine");
    require(!host_copy_amiga_name_ok(""), "empty name");
    require(!host_copy_amiga_name_ok("."), "dot");
    require(!host_copy_amiga_name_ok(".."), "dot dot");
    require(!host_copy_amiga_name_ok("OUT:payload"), "colon would select an assign");
    require(!host_copy_amiga_name_ok("a/b"), "slash would add a path component");

    require(host_copy_host_name_ok("file.txt", 1), "plain Windows name");
    require(host_copy_host_name_ok("a:b", 0), "colon is fine on POSIX");
    require(!host_copy_host_name_ok("a/b", 0), "slash on POSIX");
    require(!host_copy_host_name_ok("", 0), "empty name");
    require(!host_copy_host_name_ok("..", 0), "dot dot");
    require(!host_copy_host_name_ok("a:b", 1), "colon on Windows");
    require(!host_copy_host_name_ok("a\\b", 1), "backslash on Windows");
    require(!host_copy_host_name_ok("a*b", 1), "star on Windows");
    require(!host_copy_host_name_ok("a\"b", 1), "quote on Windows");
    require(!host_copy_host_name_ok("a.", 1), "trailing dot on Windows");
    require(!host_copy_host_name_ok("a ", 1), "trailing space on Windows");
    require(!host_copy_host_name_ok("NUL", 1), "device name");
    require(!host_copy_host_name_ok("nul.txt", 1), "device name with extension");
    require(!host_copy_host_name_ok("COM3", 1), "numbered device name");
    require(!host_copy_host_name_ok("lpt9.log", 1), "printer device name");
    require(host_copy_host_name_ok("COM10", 1), "COM10 is an ordinary name");
    require(host_copy_host_name_ok("console.txt", 1), "a name that merely starts like a device");
    require(host_copy_host_name_ok("NUL", 0), "device names are fine on POSIX");
}

static void test_parse_reply(void)
{
    char line[64];
    unsigned long value = 0;
    const char *code = NULL;

    strcpy(line, "READY\r");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_READY, "READY with CR");

    strcpy(line, "OK 4096");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_OK && value == 4096, "OK size");

    strcpy(line, "OK 0\r");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_OK && value == 0, "OK zero");

    strcpy(line, "ERR EXISTS");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_ERR, "ERR code");
    require_string(code, "EXISTS", "ERR code text");

    strcpy(line, "OK");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_INVALID, "OK without size");
    strcpy(line, "OK 12x");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_INVALID, "OK with trailing text");
    strcpy(line, "ERR ");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_INVALID, "ERR without code");
    strcpy(line, "garbage");
    require(host_copy_parse_reply(line, &value, &code) == HOST_COPY_REPLY_INVALID, "unknown line");

    require(strstr(host_copy_error_text("EXISTS"), "FORCE") != NULL, "EXISTS text mentions FORCE");
    require(strstr(host_copy_error_text("nonsense"), "error") != NULL, "unknown codes have a text");
}

/* ---- POSIX get ---- */

static void test_get_command_shape(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "/tmp/it's here.bin"),
            "get command should build");
    require_contains(command, "f='/tmp/it'\\''s here.bin';");
    require_contains(command, "[ -f \"$f\" ] || exit 2");
    require_contains(command, "wc -c <\"$f\"; cat -- \"$f\"");

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "~/My Files/x.bin"),
            "home path should build");
    require_contains(command, "f=\"$HOME\"'/My Files/x.bin';");

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "~"), "bare home should build");
    require_contains(command, "f=\"$HOME\";");

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "~user/x"), "other user path should build");
    require_contains(command, "f='~user/x';");

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "caf\xe9.bin"),
            "Latin-1 path should build");
    require_contains(command, "f='caf\xc3\xa9.bin';");

    {
        char small[16];

        small[0] = '\0';
        require(!host_append_copy_get_command(small, sizeof(small), "/tmp/x"),
                "small buffer should fail");
    }
}

static void test_get_runs(void)
{
    static unsigned char expected[300];
    static unsigned char actual[400];
    char command[HOST_MAX_COMMAND_LEN];
    char path[512];
    char out[512];
    long n;
    int rc;

    path_in(out, sizeof(out), "out.bin");

    /* every byte value, in a file whose name has a space and a quote */
    all_bytes(expected);
    path_in(path, sizeof(path), "it's a file.bin");
    write_file(path, expected, 256);
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), path), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 0, "get of an existing file should exit 0");
    n = read_file(out, actual, sizeof(actual));
    require(n == 4 + 256 && memcmp(actual, "256\n", 4) == 0, "size line then 256 bytes");
    require(memcmp(actual + 4, expected, 256) == 0, "all 256 byte values should arrive unchanged");

    /* empty file */
    path_in(path, sizeof(path), "empty.bin");
    write_file(path, expected, 0);
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), path), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 0, "get of an empty file should exit 0");
    n = read_file(out, actual, sizeof(actual));
    require(n == 2 && memcmp(actual, "0\n", 2) == 0, "empty file reports size 0 and no data");

    /* missing file */
    path_in(path, sizeof(path), "missing.bin");
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), path), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 2, "missing file should exit 2");
    require(read_file(out, actual, sizeof(actual)) == 0, "missing file prints nothing");

    /* a directory is not a regular file */
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), workdir), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 2, "directory should exit 2");

    /* Latin-1 name typed on the Amiga reaches the UTF-8 file name */
    path_in(path, sizeof(path), "caf\xc3\xa9.bin");
    write_file(path, expected, 5);
    {
        char latin1[512];

        path_in(latin1, sizeof(latin1), "caf\xe9.bin");
        command[0] = '\0';
        require(host_append_copy_get_command(command, sizeof(command), latin1), "get should build");
        rc = run_command(command, NULL, out);
        require(rc == 0, "Latin-1 name should find the UTF-8 file");
        n = read_file(out, actual, sizeof(actual));
        require(n == 2 + 5 && memcmp(actual, "5\n", 2) == 0, "Latin-1 name copies the file");
    }

    /* "~/name" resolves against HOME */
    path_in(path, sizeof(path), "homefile.bin");
    write_file(path, expected, 7);
    require(setenv("HOME", workdir, 1) == 0, "HOME should be set");
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "~/homefile.bin"), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 0, "~/ path should resolve");
    n = read_file(out, actual, sizeof(actual));
    require(n == 2 + 7 && memcmp(actual, "7\n", 2) == 0, "~/ path copies the file");
}

/* ---- POSIX put ---- */

static void test_put_command_shape(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_copy_put_command(command, sizeof(command), "/tmp/it's", "a b", 512, 0),
            "put command should build");
    require_contains(command, "p='/tmp/it'\\''s'; n='a b';");
    require_contains(command, "echo ERR EXISTS");
    require_contains(command, "head -c 512 >\"$t\"");
    require_contains(command, "if [ \"$s\" != 512 ]");
    require_contains(command, "ln -- \"$t\" \"$p\"");
    require_contains(command, "mv -f -- \"$t\" \"$p\"");
    require_contains(command, "rm -f -- \"$t\"");

    command[0] = '\0';
    require(host_append_copy_put_command(command, sizeof(command), "/tmp/x", "x", 5, 1), "forced put should build");
    require(strstr(command, "ERR EXISTS") == NULL, "FORCE must skip the existing-file check");
    require(strstr(command, "ln --") == NULL, "FORCE replaces the file, so it must not use the no-clobber link");
    require_contains(command, "mv -f -- \"$t\" \"$p\"");

    command[0] = '\0';
    require(host_append_copy_put_command(command, sizeof(command), "~/Downloads", "caf\xe9", 5, 0),
            "home path should build");
    require_contains(command, "p=\"$HOME\"/Downloads; n='caf\xc3\xa9';");
}

static void put_command(char *command, const char *target, const char *name, unsigned long len, int force)
{
    command[0] = '\0';
    require(host_append_copy_put_command(command, HOST_MAX_COMMAND_LEN, target, name, len, force),
            "put should build");
}

static void test_put_runs(void)
{
    static unsigned char data[300];
    static unsigned char actual[400];
    char command[HOST_MAX_COMMAND_LEN];
    char in[512];
    char target[512];
    char out[512];
    char sub[512];
    long n;
    int rc;

    all_bytes(data);
    path_in(in, sizeof(in), "input.bin");
    path_in(out, sizeof(out), "reply.txt");
    write_file(in, data, 256);

    /* new file with a name that needs quoting */
    path_in(target, sizeof(target), "it's a copy.bin");
    put_command(command, target, "input.bin", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 0, "put to a new file should exit 0");
    n = read_file(out, actual, sizeof(actual));
    require(n == 13 && memcmp(actual, "READY\nOK 256\n", 13) == 0, "reply is READY then OK size");
    n = read_file(target, actual, sizeof(actual));
    require(n == 256 && memcmp(actual, data, 256) == 0, "all 256 byte values should be stored unchanged");
    require(leftover_temp_files(workdir) == 0, "no temporary file may remain");

    /* existing file without FORCE */
    put_command(command, target, "input.bin", 256, 0);
    write_file(target, (const unsigned char *)"old", 3);
    rc = run_command(command, in, out);
    require(rc == 3, "existing target without FORCE should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 11 && memcmp(actual, "ERR EXISTS\n", 11) == 0, "reply is ERR EXISTS and no READY");
    n = read_file(target, actual, sizeof(actual));
    require(n == 3 && memcmp(actual, "old", 3) == 0, "existing file stays untouched");

    /* existing file with FORCE */
    put_command(command, target, "input.bin", 256, 1);
    rc = run_command(command, in, out);
    require(rc == 0, "FORCE should replace the file");
    n = read_file(target, actual, sizeof(actual));
    require(n == 256 && memcmp(actual, data, 256) == 0, "FORCE stores the new content");
    require(leftover_temp_files(workdir) == 0, "no temporary file after FORCE");

    /* empty file */
    path_in(target, sizeof(target), "empty copy.bin");
    path_in(sub, sizeof(sub), "none.bin");
    write_file(sub, data, 0);
    put_command(command, target, "none.bin", 0, 0);
    rc = run_command(command, sub, out);
    require(rc == 0, "empty file should copy");
    require(read_file(target, actual, sizeof(actual)) == 0, "empty file is created empty");
    n = read_file(out, actual, sizeof(actual));
    require(n == 11 && memcmp(actual, "READY\nOK 0\n", 11) == 0, "empty reply");

    /* a folder as target: the source name is appended */
    path_in(sub, sizeof(sub), "folder");
    require(mkdir(sub, 0755) == 0, "folder should be created");
    put_command(command, sub, "named it.bin", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 0, "put into a folder should exit 0");
    path_in(target, sizeof(target), "folder/named it.bin");
    require(read_file(target, actual, sizeof(actual)) == 256, "file lands in the folder under its own name");

    /* a trailing slash on the folder works the same */
    {
        char slash[520];

        snprintf(slash, sizeof(slash), "%s/", sub);
        unlink(target);
        put_command(command, slash, "named it.bin", 256, 0);
        rc = run_command(command, in, out);
        require(rc == 0, "folder with a trailing slash should work");
        require(read_file(target, actual, sizeof(actual)) == 256, "file lands in the folder");
    }

    /* a folder where the name would go */
    path_in(target, sizeof(target), "folder/sub");
    require(mkdir(target, 0755) == 0, "inner folder should be created");
    put_command(command, sub, "sub", 256, 1);
    rc = run_command(command, in, out);
    require(rc == 3, "a folder in the way should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 10 && memcmp(actual, "ERR ISDIR\n", 10) == 0, "reply is ERR ISDIR");

    /* a folder target with a name that cannot be used */
    put_command(command, sub, "", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 3, "empty name into a folder should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 11 && memcmp(actual, "ERR NONAME\n", 11) == 0, "reply is ERR NONAME");

    /* a trailing slash on something that is not a folder */
    path_in(target, sizeof(target), "nofolder/");
    put_command(command, target, "x", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 3, "slash on a missing folder should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 11 && memcmp(actual, "ERR NOTDIR\n", 11) == 0, "reply is ERR NOTDIR");

    /* missing parent folder */
    path_in(target, sizeof(target), "no/such/dir.bin");
    put_command(command, target, "x", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 3, "missing parent should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 9 && memcmp(actual, "ERR OPEN\n", 9) == 0, "reply is ERR OPEN");

    /* stdin ends before the announced size: nothing is stored */
    path_in(target, sizeof(target), "short.bin");
    path_in(sub, sizeof(sub), "short-in.bin");
    write_file(sub, data, 100);
    put_command(command, target, "short.bin", 256, 0);
    rc = run_command(command, sub, out);
    require(rc == 3, "short input should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 15 && memcmp(actual, "READY\nERR SIZE\n", 15) == 0, "reply is READY then ERR SIZE");
    require(!file_exists(target), "a short transfer must not create the target");
    require(leftover_temp_files(workdir) == 0, "a short transfer must not leave a temporary file");

    /* surplus input is not stored */
    path_in(target, sizeof(target), "exact.bin");
    put_command(command, target, "exact.bin", 100, 0);
    rc = run_command(command, in, out);
    require(rc == 0, "reading only the announced size should exit 0");
    n = read_file(target, actual, sizeof(actual));
    require(n == 100 && memcmp(actual, data, 100) == 0, "only the announced bytes are stored");

    /* a target that appears while the upload runs is not replaced without FORCE */
    path_in(target, sizeof(target), "race.bin");
    {
        char producer[1200];

        snprintf(producer, sizeof(producer),
                 "while ! ls %s/.htput.* >/dev/null 2>&1; do sleep 0.05; done; printf winner >%s",
                 workdir, target);
        run_producer = producer;
        put_command(command, target, "race.bin", 256, 0);
        rc = run_command(command, in, out);
        run_producer = NULL;
    }
    require(rc == 3, "a target created during the upload should exit 3");
    n = read_file(out, actual, sizeof(actual));
    require(n == 17 && memcmp(actual, "READY\nERR EXISTS\n", 17) == 0, "reply is READY then ERR EXISTS");
    n = read_file(target, actual, sizeof(actual));
    require(n == 6 && memcmp(actual, "winner", 6) == 0, "the file created during the upload must stay");
    require(leftover_temp_files(workdir) == 0, "no temporary file after losing the race");

    /* with FORCE the same race replaces the file, as asked */
    {
        char producer[1200];

        unlink(target);
        snprintf(producer, sizeof(producer),
                 "while ! ls %s/.htput.* >/dev/null 2>&1; do sleep 0.05; done; printf winner >%s",
                 workdir, target);
        run_producer = producer;
        put_command(command, target, "race.bin", 256, 1);
        rc = run_command(command, in, out);
        run_producer = NULL;
    }
    require(rc == 0, "FORCE should replace a target created during the upload");
    require(read_file(target, actual, sizeof(actual)) == 256, "FORCE stores the uploaded content");

    /* relative names that start with a dash are operands, not options */
    run_cwd = workdir;
    path_in(target, sizeof(target), "-dash.bin");
    put_command(command, "-dash.bin", "x", 256, 0);
    rc = run_command(command, in, out);
    require(rc == 0, "a relative target starting with a dash should copy");
    require(read_file(target, actual, sizeof(actual)) == 256 && leftover_temp_files(workdir) == 0,
            "the dash-named file is stored and no temporary file remains");
    put_command(command, "-dash.bin", "x", 256, 1);
    rc = run_command(command, in, out);
    require(rc == 0, "FORCE over a dash-named file should copy");
    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "-dash.bin"), "get should build");
    rc = run_command(command, NULL, out);
    require(rc == 0, "get of a relative dash-named file should exit 0");
    n = read_file(out, actual, sizeof(actual));
    require(n == 4 + 256 && memcmp(actual, "256\n", 4) == 0, "dash-named file is read");
    run_cwd = NULL;

    /* text a guest already sends as UTF-8 is not converted again */
    path_in(target, sizeof(target), "caf\xc3\xa9 utf8.bin");
    {
        char guest_utf8[512];

        path_in(guest_utf8, sizeof(guest_utf8), "caf\xc3\xa9 utf8.bin");
        put_command(command, guest_utf8, "x", 256, 0);
        rc = run_command(command, in, out);
        require(rc == 0, "UTF-8 target should copy");
        require(read_file(target, actual, sizeof(actual)) == 256, "the UTF-8 name is used as written");
        path_in(target, sizeof(target), "caf\xc3\x83\xc2\xa9 utf8.bin");
        require(!file_exists(target), "no double-encoded name may be created");
    }

    /* Latin-1 target name reaches the UTF-8 file name */
    {
        char latin1[512];

        path_in(latin1, sizeof(latin1), "caf\xe9 copy.bin");
        put_command(command, latin1, "x", 256, 0);
        rc = run_command(command, in, out);
        require(rc == 0, "Latin-1 target should copy");
        path_in(target, sizeof(target), "caf\xc3\xa9 copy.bin");
        require(read_file(target, actual, sizeof(actual)) == 256, "UTF-8 file name was created");
    }
}

/* ---- Windows (PowerShell) ---- */

/* Decode the -EncodedCommand payload of a Windows command back to text. */
static void decode_ps(const char *command, char *script, size_t script_size)
{
    static const char prefix[] = "powershell -NoProfile -EncodedCommand ";
    static unsigned char decoded[HOST_MAX_COMMAND_LEN * 2];
    struct host_base64_state st;
    long n;

    require(strncmp(command, prefix, sizeof(prefix) - 1) == 0, "command should start with the powershell prefix");
    host_base64_init(&st);
    n = host_base64_feed(&st, command + sizeof(prefix) - 1, (long)strlen(command + sizeof(prefix) - 1), decoded);
    require(n > 0 && n % 2 == 0 && (size_t)n / 2 < script_size, "decoded script should fit");
    for (long i = 0; i < n / 2; i++) {
        require(decoded[i * 2 + 1] == 0, "script characters should be ISO-8859-1");
        script[i] = (char)decoded[i * 2];
    }
    script[n / 2] = '\0';
}

static void test_windows_get(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    static char script[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_copy_get_command_windows(command, sizeof(command), "C:\\Users\\it's me\\caf\xe9.bin"),
            "Windows get should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "$ErrorActionPreference='Stop';");
    require_contains(script, "Get-Item -Force -LiteralPath 'C:\\Users\\it''s me\\caf\xe9.bin'}catch{exit 2}");
    require_contains(script, "-isnot [IO.FileInfo]");
    require_contains(script, "[Console]::OpenStandardOutput()");
    require_contains(script, "$s.CopyTo($o)");

    command[0] = '\0';
    require(host_append_copy_get_command_windows(command, sizeof(command), "~\\Documents\\a.txt"),
            "home path should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "-LiteralPath ([Environment]::GetFolderPath('UserProfile')+'\\Documents\\a.txt')}");

    command[0] = '\0';
    require(host_append_copy_get_command_windows(command, sizeof(command), "~/a.txt"), "home slash path should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "GetFolderPath('UserProfile')+'/a.txt')");

    command[0] = '\0';
    require(host_append_copy_get_command_windows(command, sizeof(command), "~"), "bare home should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "-LiteralPath ([Environment]::GetFolderPath('UserProfile'))}");

    command[0] = '\0';
    require(host_append_copy_get_command_windows(command, sizeof(command), "~other\\a.txt"), "other user path should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "-LiteralPath '~other\\a.txt'}");
}

static void test_windows_put(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    static char script[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_copy_put_command_windows(command, sizeof(command), "C:\\Out\\it's", "caf\xe9 'x'.bin",
                                                 123456, 0),
            "Windows put should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "$p='C:\\Out\\it''s';$n='caf\xe9 ''x''.bin';$l=123456;$f=0;");
    require_contains(script, "'.htput.'+$PID");
    require_contains(script, "[Console]::OpenStandardInput()");
    require_contains(script, "[Console]::Out.Write(\"READY`n\")");
    require_contains(script, "Write(\"OK $l`n\")");
    require_contains(script, "if(!$f -and [IO.File]::Exists($p)){$e='EXISTS';throw 1}");
    require_contains(script, "if($f){Move-Item -Force -LiteralPath $t -Destination $p}else{[IO.File]::Move($t,$p)};");
    require_contains(script, "if($e -eq 'MOVE' -and !$f -and [IO.File]::Exists($p)){$e='EXISTS'}");
    require_contains(script, "if($d -and $r -gt 0){try{");

    /* UTF-8 text from a guest is decoded once for the ISO-8859-1 script */
    command[0] = '\0';
    require(host_append_copy_put_command_windows(command, sizeof(command), "C:\\Out\\caf\xc3\xa9", "na\xc3\xafve.bin", 5, 0),
            "UTF-8 Windows put should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "$p='C:\\Out\\caf\xe9';$n='na\xefve.bin';");

    command[0] = '\0';
    require(host_append_copy_put_command_windows(command, sizeof(command), "~\\x", "a", 0, 1), "forced put should build");
    decode_ps(command, script, sizeof(script));
    require_contains(script, "$p=([Environment]::GetFolderPath('UserProfile')+'\\x');$n='a';$l=0;$f=1;");

    /* a folder and file name that together stay under the Windows MAX_PATH of 260 fit the 4096-byte trap buffer */
    {
        char longpath[241];
        char longname[101];

        memset(longpath, 'p', sizeof(longpath) - 1);
        longpath[sizeof(longpath) - 1] = '\0';
        memset(longname, 'n', sizeof(longname) - 1);
        longname[sizeof(longname) - 1] = '\0';
        command[0] = '\0';
        require(host_append_copy_put_command_windows(command, sizeof(command), longpath, longname,
                                                     2147483647UL, 1),
                "a 240 character path and 100 character name should fit");
        require(strlen(command) < HOST_MAX_COMMAND_LEN, "command stays below the trap limit");
    }

    {
        char huge[HOST_MAX_COMMAND_LEN];

        memset(huge, 'p', sizeof(huge) - 1);
        huge[sizeof(huge) - 1] = '\0';
        command[0] = '\0';
        require(!host_append_copy_put_command_windows(command, sizeof(command), huge, "a", 1, 0),
                "an oversized path should fail instead of truncating");
    }
}

static void test_posix_syntax(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char check[HOST_MAX_COMMAND_LEN * 2];

    put_command(command, "~/a b/it's", "n'ame", 99, 0);
    check[0] = '\0';
    require(host_append_literal(check, sizeof(check), "sh -n -c ") &&
            host_append_shell_arg(check, sizeof(check), command, 0),
            "syntax check line should fit");
    require(system(check) == 0, "generated put command should parse");

    command[0] = '\0';
    require(host_append_copy_get_command(command, sizeof(command), "~/a b/it's"), "get should build");
    check[0] = '\0';
    require(host_append_literal(check, sizeof(check), "sh -n -c ") &&
            host_append_shell_arg(check, sizeof(check), command, 0),
            "syntax check line should fit");
    require(system(check) == 0, "generated get command should parse");
}

int main(void)
{
    char cleanup[320];

    test_latin1_to_utf8();
    test_text_encoding();
    test_put_events();
    test_names();
    test_parse_reply();
    test_get_command_shape();
    test_put_command_shape();
    test_windows_get();
    test_windows_put();

    make_workdir();
    test_posix_syntax();
    test_get_runs();
    test_put_runs();

    snprintf(cleanup, sizeof(cleanup), "rm -rf %s", workdir);
    require(system(cleanup) == 0, "work directory should be removed");
    return 0;
}
