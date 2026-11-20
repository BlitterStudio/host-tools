/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "host_base64.h"
#include "host_clip_command.h"
#include "host_edit_command.h"
#include "host_env_command.h"
#include "host_info_command.h"
#include "host_multiview_command.h"
#include "host_notify_command.h"
#include "host_powershell.h"
#include "host_reveal_command.h"
#include "host_run_command.h"
#include "host_shell_command.h"

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
    require_contains(command, "org.freedesktop.FileManager1.ShowItems");
    require_contains(command, "file:///tmp/a%20b%27s.txt");
    require_contains(command, "xdg-open \"$(dirname -- '/tmp/a b'\\''s.txt')\"");
    require_contains(command, "else exit 127; fi");
    require_shell_syntax(command);
}

static void test_reveal_command_without_uri(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_reveal_command(command, sizeof(command), "relative/path.txt"),
            "reveal command should build without a file URI");

    require_contains(command, "open -R relative/path.txt");
    require_contains(command, "xdg-open \"$(dirname -- relative/path.txt)\"");
    require(strstr(command, "ShowItems") == NULL,
            "reveal command should skip the file manager branch without a URI");
    require_shell_syntax(command);
}

static void test_notify_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_notify_command(command, sizeof(command),
                                       "Build's Done", "Hello $USER & goodbye"),
            "notify command should build");

    require_contains(command, "t='Build'\\''s Done'");
    require_contains(command, "m='Hello $USER & goodbye'");
    require_contains(command, "iconv -f ISO-8859-1 -t UTF-8");
    require_contains(command, "notify-send -- \"$t\" \"$m\"");
    require_contains(command, "osascript - \"$t\" \"$m\"; else exit 127; fi");
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
    require_contains(command, "iconv -f ISO-8859-1 -t UTF-8");
    require_contains(command, "pbcopy");
    require_contains(command, "wl-copy");
    require_contains(command, "xclip -selection clipboard");
    require_contains(command, "xsel --clipboard --input");
    require_shell_syntax(command);
}

static void test_clip_copy_command_multiline(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_clip_copy_command(command, sizeof(command),
                                          "line one\nline two\n"),
            "multiline clipboard copy command should build");

    require_contains(command, "printf %s 'line one\nline two\n'");
    require_shell_syntax(command);
}

static void test_clip_paste_command(void)
{
    char command[HOST_MAX_COMMAND_LEN * 2];
    char output[32];
    FILE *pipe;
    size_t output_len;

    require_contains(HOST_CLIP_PASTE_COMMAND, "pbpaste");
    require_contains(HOST_CLIP_PASTE_COMMAND, "wl-paste -n");
    require_contains(HOST_CLIP_PASTE_COMMAND, "xclip -selection clipboard -o");
    require_contains(HOST_CLIP_PASTE_COMMAND, "xsel --clipboard --output");
    require_contains(HOST_CLIP_PASTE_COMMAND, "iconv -c -f UTF-8 -t ISO-8859-1//TRANSLIT");
    require_contains(HOST_CLIP_PASTE_COMMAND, "t=$(mktemp)");
    require(strstr(HOST_CLIP_PASTE_COMMAND, "out=$(") == NULL,
            "clipboard paste must not use command substitution for clipboard data");
    require_shell_syntax(HOST_CLIP_PASTE_COMMAND);

    command[0] = '\0';
    require(host_append_literal(command, sizeof(command),
                                "pbpaste() { printf 'line\\n\\n'; }; "),
            "clipboard behavior prefix should fit");
    require(host_append_literal(command, sizeof(command), HOST_CLIP_PASTE_COMMAND),
            "clipboard behavior command should fit");
    pipe = popen(command, "r");
    require(pipe != NULL, "clipboard behavior command should start");
    output_len = fread(output, 1, sizeof(output), pipe);
    require(pclose(pipe) == 0, "clipboard behavior command should succeed");
    require(output_len == 6 && memcmp(output, "line\n\n", 6) == 0,
            "clipboard paste must preserve trailing newlines");
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

static void test_env_name_validation(void)
{
    require(host_env_valid_name("FOO"), "simple env name should be valid");
    require(host_env_valid_name("_FOO_1"), "underscore env name should be valid");
    require(!host_env_valid_name(""), "empty env name should be invalid");
    require(!host_env_valid_name("1FOO"), "env name must not start with a digit");
    require(!host_env_valid_name("BAD-NAME"), "env name must not contain hyphens");
    require(!host_env_valid_name("BAD=NAME"), "env name must not contain equals");
    require(host_env_valid_value("one line"), "single-line env value should be valid");
    require(host_env_valid_value(""), "empty env value should be valid");
    require(!host_env_valid_value("line one\nline two"),
            "env value must not contain line feeds");
    require(!host_env_valid_value("line one\rline two"),
            "env value must not contain carriage returns");
}

static void test_env_get_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_get_command(command, sizeof(command), "FOO"),
            "env get command should build");

    require_contains(command, "umask 077; set -- \"${HOME:?}/.host-tools-env\" '' ''; ");
    require_contains(command, "if [ \"${FOO+x}\" = x ]; then printf '%s\\n' \"$FOO\"");
    require_contains(command, ". \"$1\"");
    require(strstr(command, "$f") == NULL && strstr(command, "f=") == NULL,
            "env get must not use a scratch variable that a lookup of f would find");
    require_contains(command, "else exit 1; fi");
    require_shell_syntax(command);
}

static void test_env_get_command_output(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char wrapped[HOST_MAX_COMMAND_LEN * 2];
    char output[64];
    FILE *pipe;
    size_t output_len;

    command[0] = '\0';
    require(host_append_env_get_command(command, sizeof(command), "HOST_TOOLS_TEST"),
            "env get command should build for behavior test");

    wrapped[0] = '\0';
    require(host_append_literal(wrapped, sizeof(wrapped),
                                "HOST_TOOLS_TEST='value with spaces' sh -c "),
            "env get behavior prefix should fit");
    require(host_append_shell_arg(wrapped, sizeof(wrapped), command, 0),
            "env get behavior command should fit");

    pipe = popen(wrapped, "r");
    require(pipe != NULL, "env get behavior command should start");
    output_len = fread(output, 1, sizeof(output) - 1, pipe);
    output[output_len] = '\0';
    require(pclose(pipe) == 0, "env get behavior command should succeed");
    require_string(output, "value with spaces\n",
                   "env get should print the value as one complete line");
}

static void test_env_set_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_set_command(command, sizeof(command),
                                        "FOO", "a b's $HOME"),
            "env set command should build");

    require_contains(command, "umask 077; set -- \"${HOME:?}/.host-tools-env\" '' ''; ");
    require_contains(command, "trap 'rm -f \"$3\"; [ -z \"$2\" ] || rmdir \"$2\"' 0; trap 'exit 1' 1 2 15");
    require_contains(command, "flock -n 9");
    require_contains(command, "mkdir \"$1.lock.d\"");
    require_contains(command, "t=$(mktemp \"$1.XXXXXX\")");
    require_contains(command, "grep -v '^export FOO=' \"$1\"");
    require_contains(command, "printf '%s\\n' 'export FOO='\\''a b'\\''\\'\\'''\\''s $HOME'\\'''");
    require_contains(command, "mv \"$3\" \"$1\"");
    require_contains(command, "chmod 600 \"$3\"");
    require_shell_syntax(command);

    command[0] = '\0';
    require(!host_append_env_set_command(command, sizeof(command), "FOO", "one\ntwo"),
            "multiline env values should be rejected");
}

static void test_env_unset_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_unset_command(command, sizeof(command), "FOO"),
            "env unset command should build");

    require_contains(command, "umask 077; set -- \"${HOME:?}/.host-tools-env\" '' ''; ");
    require_contains(command, "flock -n 9");
    require_contains(command, "t=$(mktemp \"$1.XXXXXX\")");
    require_contains(command, "grep -v '^export FOO=' \"$1\" > \"$3\"");
    require_contains(command, "mv \"$3\" \"$1\"");
    require_contains(command, "chmod 600 \"$3\"");
    require_shell_syntax(command);
}

static void test_env_set_behavior(void)
{
    char temp_dir[] = "/tmp/host-tools-env.XXXXXX";
    char env_path[256];
    char command[HOST_MAX_COMMAND_LEN];
    char invoke[HOST_MAX_COMMAND_LEN * 4];
    char contents[128];
    struct stat st;
    FILE *env_file;
    size_t contents_len;

    require(mkdtemp(temp_dir) != NULL, "env behavior temp directory should be created");
    command[0] = '\0';
    require(host_append_env_set_command(command, sizeof(command), "FOO", "private value"),
            "env behavior command should build");

    invoke[0] = '\0';
    require(host_append_literal(invoke, sizeof(invoke), "HOME="),
            "env behavior HOME prefix should fit");
    require(host_append_shell_arg(invoke, sizeof(invoke), temp_dir, 0),
            "env behavior HOME should fit");
    require(host_append_literal(invoke, sizeof(invoke), " sh -c "),
            "env behavior shell prefix should fit");
    require(host_append_shell_arg(invoke, sizeof(invoke), command, 0),
            "env behavior shell command should fit");
    require(system(invoke) == 0, "env behavior command should succeed");

    require(snprintf(env_path, sizeof(env_path), "%s/.host-tools-env", temp_dir) > 0,
            "env behavior path should build");
    require(stat(env_path, &st) == 0, "env file should exist");
    require((st.st_mode & 0777) == 0600, "env file should be owner-readable only");

    env_file = fopen(env_path, "r");
    require(env_file != NULL, "env file should be readable");
    contents_len = fread(contents, 1, sizeof(contents) - 1, env_file);
    require(fclose(env_file) == 0, "env file should close");
    contents[contents_len] = '\0';
    require(strcmp(contents, "export FOO='private value'\n") == 0,
            "env file should contain the requested export");

    require(unlink(env_path) == 0, "env behavior file should be removed");
    /* the lock file (flock) stays behind; the lock directory (mkdir) must not */
    snprintf(env_path, sizeof(env_path), "%s/.host-tools-env.lock", temp_dir);
    unlink(env_path);
    require(rmdir(temp_dir) == 0, "env behavior temp directory should be removed");
}

static void test_env_list_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_list_command(command, sizeof(command)),
            "env list command should build");

    require_contains(command, "set -- \"${HOME:?}/.host-tools-env\" '' ''");
    require_contains(command, "env");
    require_contains(command, ". \"$1\"");
    require_contains(command, "sort");
    require_shell_syntax(command);
}

static void test_shell_login_interactive_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_shell_login_command(command, sizeof(command), ""),
            "interactive shell login command should build");

    require_string(command, "h=\"${SHELL:-/bin/sh}\"; exec \"$h\" -l",
                   "interactive shell should exec the user's login shell");
    require_shell_syntax(command);
}

static void test_shell_login_explicit_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_shell_login_command(command, sizeof(command), "printf '%s\\n' \"$PATH\""),
            "explicit shell login command should build");

    require_string(command,
                   "h=\"${SHELL:-/bin/sh}\"; exec \"$h\" -l -c 'printf '\\''%s\\n'\\'' \"$PATH\"'",
                   "explicit command should run through the user's login shell");
    require_shell_syntax(command);
}

static void test_shell_login_command_limit(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char explicit_command[HOST_MAX_COMMAND_LEN];

    memset(explicit_command, 'x', sizeof(explicit_command) - 1);
    explicit_command[sizeof(explicit_command) - 1] = '\0';
    command[0] = '\0';
    require(!host_append_shell_login_command(command, sizeof(command), explicit_command),
            "login wrapper must reject a command that exceeds the trap buffer");
}

/*
 * Decode the -EncodedCommand payload (UTF-16LE) of a PowerShell command
 * into UTF-8, independently of the encoder under test.
 */
static void decode_ps_command(const char *command, char *out, size_t out_size)
{
    static const char prefix[] = "powershell -NoProfile -EncodedCommand ";
    struct host_base64_state st;
    static unsigned char decoded[HOST_MAX_COMMAND_LEN * 4];
    long n;
    size_t pos = 0;

    require(strncmp(command, prefix, sizeof(prefix) - 1) == 0,
            "encoded command should start with the powershell prefix");

    host_base64_init(&st);
    n = host_base64_feed(&st, command + sizeof(prefix) - 1,
                         (long)strlen(command + sizeof(prefix) - 1), decoded);
    require(n >= 0 && n % 2 == 0, "decoded UTF-16 should hold whole code units");

    for (long i = 0; i < n; i += 2) {
        unsigned long unit = decoded[i] | ((unsigned long)decoded[i + 1] << 8);
        unsigned long cp = unit;

        if (unit >= 0xD800 && unit <= 0xDBFF) {
            unsigned long low;

            require(i + 3 < n, "high surrogate should be followed by a low surrogate");
            low = decoded[i + 2] | ((unsigned long)decoded[i + 3] << 8);
            require(low >= 0xDC00 && low <= 0xDFFF, "low surrogate expected");
            cp = 0x10000 + ((unit - 0xD800) << 10) + (low - 0xDC00);
            i += 2;
        }
        require(pos + 5 < out_size, "decoded script buffer should be large enough");
        if (cp < 0x80) {
            out[pos++] = (char)cp;
        } else if (cp < 0x800) {
            out[pos++] = (char)(0xC0 | (cp >> 6));
            out[pos++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out[pos++] = (char)(0xE0 | (cp >> 12));
            out[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[pos++] = (char)(0x80 | (cp & 0x3F));
        } else {
            out[pos++] = (char)(0xF0 | (cp >> 18));
            out[pos++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[pos++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[pos++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    out[pos] = '\0';
}

/* The script (UTF-8) that the command runs inside the error-reporting wrapper */
static void require_ps_script(const char *command, const char *script)
{
    static char decoded[HOST_MAX_COMMAND_LEN * 2];
    static char expected[HOST_MAX_COMMAND_LEN * 2];

    decode_ps_command(command, decoded, sizeof(decoded));
    snprintf(expected, sizeof(expected), "%s%s%s", HOST_PS_TRY, script, HOST_PS_CATCH);
    require_string(decoded, expected, "decoded PowerShell script");
}

static void test_ps_quoting(void)
{
    char dest[64];

    dest[0] = '\0';
    require(host_append_ps_quoted(dest, sizeof(dest), "it's"),
            "powershell quoting should build");
    require(strcmp(dest, "'it''s'") == 0,
            "powershell quoting should double embedded quotes");

    dest[0] = '\0';
    require(!host_append_ps_quoted(dest, 4, "long text"),
            "small powershell quote buffer should fail");
}

static void test_ps_encoding(void)
{
    char dest[64];
    char out[64];
    char command[HOST_MAX_COMMAND_LEN];
    static char decoded[HOST_MAX_COMMAND_LEN * 2];

    /* guest text and NativeDosOp paths are ISO-8859-1 and become UTF-8 in the script */
    dest[0] = '\0';
    require(host_append_ps_quoted(dest, sizeof(dest), "caf\xE9 \xFF's"),
            "guest text should quote");
    require_string(dest, "'caf\xC3\xA9 \xC3\xBF''s'", "guest text should become UTF-8");

    dest[0] = '\0';
    require(!host_append_ps_quoted(dest, 5, "caf\xE9"),
            "guest text that expands past the buffer should fail");
    require_string(dest, "", "failed quoting should leave the buffer unchanged");

    /* UTF-8 to UTF-16LE, including a surrogate pair */
    require(host_base64_encode_utf16le("\xC3\xA9", out, sizeof(out)), "e acute should encode");
    require_string(out, "6QA=", "U+00E9 is E9 00");
    require(host_base64_encode_utf16le("\xE2\x82\xAC", out, sizeof(out)), "euro should encode");
    require_string(out, "rCA=", "U+20AC is AC 20");
    require(host_base64_encode_utf16le("\xF0\x9F\x98\x80", out, sizeof(out)),
            "astral character should encode");
    require_string(out, "PdgA3g==", "U+1F600 is D83D DE00");
    require(!host_base64_encode_utf16le("caf\xE9", out, sizeof(out)),
            "ISO-8859-1 bytes are not valid UTF-8 and must not be encoded");
    require(!host_base64_encode_utf16le("\xE0\x80\x80", out, sizeof(out)) &&
            !host_base64_encode_utf16le("\xED\xA0\x80", out, sizeof(out)) &&
            !host_base64_encode_utf16le("\xE2\x82", out, sizeof(out)),
            "overlong, surrogate and truncated sequences should be rejected");

    /* the whole script survives the round trip and is wrapped to report errors */
    command[0] = '\0';
    require(host_append_ps_encoded_command(command, sizeof(command),
                                           "'caf\xC3\xA9 \xCE\xB1 \xF0\x9F\x98\x80'"),
            "non-ASCII script should encode");
    decode_ps_command(command, decoded, sizeof(decoded));
    require_string(decoded,
                   "try{'caf\xC3\xA9 \xCE\xB1 \xF0\x9F\x98\x80'}catch{"
                   "[Console]::OutputEncoding=[Text.Encoding]::GetEncoding(28591);"
                   "[Console]::Out.WriteLine($_.Exception.Message);exit 1}",
                   "script should be wrapped and survive UTF-16 encoding");

    command[0] = '\0';
    require(!host_append_ps_encoded_command(command, sizeof(command), "'caf\xE9'"),
            "a script with invalid UTF-8 must be rejected, not garbled");

    /* existing builders keep their call sites: guest text still reaches PowerShell intact */
    command[0] = '\0';
    require(host_append_clip_copy_command_windows(command, sizeof(command), "caf\xE9 \xDF"),
            "windows copy of ISO-8859-1 text should build");
    require_ps_script(command, "Set-Clipboard -Value 'caf\xC3\xA9 \xC3\x9F'");
}

static void test_windows_clip_commands(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_clip_paste_command_windows(command, sizeof(command)),
            "windows paste command should build");
    require_ps_script(command, HOST_CLIP_PASTE_PS_SCRIPT);

    command[0] = '\0';
    require(host_append_clip_copy_command_windows(command, sizeof(command), "a'b\nc"),
            "windows copy command should build");
    require_ps_script(command, "Set-Clipboard -Value 'a''b\nc'");
}

static void test_windows_reveal_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_reveal_command_windows(command, sizeof(command),
                                               "C:\\My Files\\disk.adf"),
            "windows reveal command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'explorer.exe' "
                      "-ArgumentList '/select,\"C:\\My Files\\disk.adf\"'");
    require(strstr(command, "explorer /select") == NULL,
            "windows reveal must not go through cmd.exe");

    /* a percent sign is data now, not an environment variable; the ISO-8859-1
     * path (as NativeDosOp returns it) reaches PowerShell as UTF-8 */
    command[0] = '\0';
    require(host_append_reveal_command_windows(command, sizeof(command),
                                               "C:\\Files\\%USERNAME% caf\xE9 \xDF.txt"),
            "windows reveal command should keep percent signs and accents");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'explorer.exe' "
                      "-ArgumentList '/select,\"C:\\Files\\%USERNAME% caf\xC3\xA9 \xC3\x9F.txt\"'");

    command[0] = '\0';
    require(!host_append_reveal_command_windows(command, sizeof(command), "C:\\a\"b.txt"),
            "windows reveal should reject a quote in the path");
}

static void test_windows_info_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_info_command_windows(command, sizeof(command)),
            "windows info command should build");
    require_ps_script(command, HOST_INFO_PS_SCRIPT);
}

static void test_windows_env_commands(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_get_command_windows(command, sizeof(command), "FOO"),
            "windows env get command should build");
    require_ps_script(command,
                      "[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(28591);"
                      "$v=[Environment]::GetEnvironmentVariable('FOO','User');"
                      "if($null -eq $v){exit 1};[Console]::Out.WriteLine($v)");

    command[0] = '\0';
    require(host_append_env_set_command_windows(command, sizeof(command),
                                                "FOO", "a'b"),
            "windows env set command should build");
    require_ps_script(command,
                      "[Environment]::SetEnvironmentVariable('FOO','a''b','User')");

    command[0] = '\0';
    require(host_append_env_unset_command_windows(command, sizeof(command), "FOO"),
            "windows env unset command should build");
    require_ps_script(command,
                      "[Environment]::SetEnvironmentVariable('FOO',$null,'User')");

    command[0] = '\0';
    require(host_append_env_list_command_windows(command, sizeof(command)),
            "windows env list command should build");
    require_ps_script(command,
                      "[Console]::OutputEncoding=[System.Text.Encoding]::GetEncoding(28591);"
                      "[Environment]::GetEnvironmentVariables('User').GetEnumerator()|"
                      "Sort-Object Name|ForEach-Object{[Console]::Out.WriteLine(($_.Name)+'='+($_.Value))}");
}

static void require_cmdline_arg(const char *arg, const char *expected)
{
    char dest[64];

    dest[0] = '\0';
    require(host_append_cmdline_arg(dest, sizeof(dest), arg, 0),
            "command line argument should fit");
    require_string(dest, expected, "command line argument quoting");
}

/* Split a command line the way CommandLineToArgvW does */
static int split_cmdline(const char *line, char args[][64], int max_args)
{
    const char *p = line;
    int count = 0;

    for (;;) {
        char *out;
        int in_quotes = 0;

        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            return count;
        }
        require(count < max_args, "too many arguments in parsed command line");
        out = args[count++];

        for (;;) {
            size_t backslashes = 0;

            while (*p == '\\') {
                backslashes++;
                p++;
            }
            if (*p == '"') {
                for (size_t i = 0; i < backslashes / 2; i++) {
                    *out++ = '\\';
                }
                if (backslashes % 2) {
                    *out++ = '"';
                } else {
                    in_quotes = !in_quotes;
                }
                p++;
                continue;
            }
            for (size_t i = 0; i < backslashes; i++) {
                *out++ = '\\';
            }
            if (*p == '\0' || (!in_quotes && (*p == ' ' || *p == '\t'))) {
                break;
            }
            *out++ = *p++;
        }
        *out = '\0';
    }
}

static void test_cmdline_arg_quoting(void)
{
    static const char *const nasty[] = {
        "", "plain", "with space", "tab\there", "say \"hi\"", "C:\\dir\\",
        "C:\\my dir\\", "a\\\"b", "a\\\\\"b", "\\\\server\\share name\\", "\"", "\\",
        "end\\\\", "a\\b c", "\" \"", "x\\\\ \\\\\\\"y"
    };
    char line[HOST_MAX_COMMAND_LEN];
    char parsed[32][64];
    int count = (int)(sizeof(nasty) / sizeof(nasty[0]));
    char dest[6];

    require_cmdline_arg("", "\"\"");
    require_cmdline_arg("plain", "plain");
    require_cmdline_arg("a b", "\"a b\"");
    require_cmdline_arg("say \"hi\"", "\"say \\\"hi\\\"\"");
    require_cmdline_arg("C:\\dir\\", "C:\\dir\\");
    require_cmdline_arg("C:\\my dir\\", "\"C:\\my dir\\\\\"");
    require_cmdline_arg("a\\\"b", "\"a\\\\\\\"b\"");
    require_cmdline_arg("a\\b c", "\"a\\b c\"");

    line[0] = '\0';
    for (int i = 0; i < count; i++) {
        require(host_append_cmdline_arg(line, sizeof(line), nasty[i], i > 0),
                "command line should fit");
    }
    require(split_cmdline(line, parsed, 32) == count,
            "quoted command line should split into the original argument count");
    for (int i = 0; i < count; i++) {
        require_string(parsed[i], nasty[i], "command line arguments should round trip");
    }

    strcpy(dest, "ab");
    require(!host_append_cmdline_arg(dest, sizeof(dest), "c d", 1),
            "command line argument that does not fit should fail");
    require_string(dest, "ab", "failed command line argument should leave the buffer unchanged");
}

static void test_windows_notify_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    static char message[1200];

    command[0] = '\0';
    require(host_append_notify_command_windows(command, sizeof(command), "Amiga", "it's done"),
            "windows notify command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "$m=[Windows.UI.Notifications.ToastNotificationManager,Windows.UI.Notifications,ContentType=WindowsRuntime];"
                      "$x=$m::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02);"
                      "$e=$x.GetElementsByTagName('text');"
                      "[void]$e.Item(0).AppendChild($x.CreateTextNode('Amiga'));"
                      "[void]$e.Item(1).AppendChild($x.CreateTextNode('it''s done'));"
                      "$m::CreateToastNotifier('{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\WindowsPowerShell\\v1.0\\powershell.exe')"
                      ".Show([Windows.UI.Notifications.ToastNotification]::new($x))");

    /* the size limit quoted in host-notify's error message */
    memset(message, 'm', 840);
    command[0] = '\0';
    require(host_append_notify_command_windows(command, sizeof(command), "Amiga", message),
            "windows notify command should fit about 850 characters");
    memset(message, 'm', sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
    command[0] = '\0';
    require(!host_append_notify_command_windows(command, sizeof(command), "Amiga", message),
            "windows notify command over the encoded size limit should fail");
}

static void test_windows_edit_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_edit_command_windows(command, sizeof(command),
                                             "C:\\Users\\Jos\xE9\\\xDF b.txt"),
            "windows edit command should build for an ISO-8859-1 path");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'notepad.exe' "
                      "-ArgumentList '\"C:\\Users\\Jos\xC3\xA9\\\xC3\x9F b.txt\"'");

    command[0] = '\0';
    require(host_append_edit_command_windows(command, sizeof(command),
                                             "C:\\My Files\\it's.txt"),
            "windows edit command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'notepad.exe' -ArgumentList '\"C:\\My Files\\it''s.txt\"'");

    command[0] = '\0';
    require(host_append_edit_command_windows(command, sizeof(command), "C:\\dir\\a.txt"),
            "windows edit command without spaces should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'notepad.exe' -ArgumentList 'C:\\dir\\a.txt'");
}

static void test_windows_multiview_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_multiview_command_windows(command, sizeof(command),
                                                  "C:\\Users\\Jos\xE9\\\xDF it's.png"),
            "windows multiview command should build for an ISO-8859-1 path");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'C:\\Users\\Jos\xC3\xA9\\\xC3\x9F it''s.png'");

    command[0] = '\0';
    require(host_append_multiview_command_windows(command, sizeof(command),
                                                  "C:\\My Files\\it's.png"),
            "windows multiview command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'C:\\My Files\\it''s.png'");

    command[0] = '\0';
    require(host_append_multiview_command_windows(command, sizeof(command),
                                                  "https://example.com/a?b=1&c=2"),
            "windows multiview URL command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'https://example.com/a?b=1&c=2'");
}

static void test_windows_run_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char arguments[256];

    command[0] = '\0';
    require(host_append_run_command_windows(command, sizeof(command), "C:\\Tools\\app.exe", "", 1),
            "windows run command without arguments should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'C:\\Tools\\app.exe'");

    arguments[0] = '\0';
    require(host_append_cmd_word(arguments, sizeof(arguments), "-f", 0) &&
            host_append_cmd_word(arguments, sizeof(arguments), "C:\\My Files\\a.txt", 1) &&
            host_append_cmd_word(arguments, sizeof(arguments), "it's & more", 1) &&
            host_append_cmd_word(arguments, sizeof(arguments), "", 1),
            "windows run arguments should build");
    command[0] = '\0';
    require(host_append_run_command_windows(command, sizeof(command), "notepad", arguments, 1),
            "windows run command with arguments should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'notepad' -ArgumentList "
                      "'-f \"C:\\My Files\\a.txt\" \"it''s & more\" \"\"'");

    /* native paths are ISO-8859-1, like guest text, and reach the script as UTF-8 */
    command[0] = '\0';
    require(host_append_run_command_windows(command, sizeof(command),
                                            "C:\\Users\\Jos\xE9\\\xDF.exe", "x", 1),
            "windows run command with an ISO-8859-1 path should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Start-Process -FilePath 'C:\\Users\\Jos\xC3\xA9\\\xC3\x9F.exe' "
                      "-ArgumentList 'x'");

    /* arguments cmd.exe cannot carry: native programs run, batch files are refused */
    arguments[0] = '\0';
    require(host_append_cmdline_arg(arguments, sizeof(arguments), "say \"hi\"", 0) &&
            host_append_cmdline_arg(arguments, sizeof(arguments), "100%", 1),
            "C runtime arguments should build");
    command[0] = '\0';
    require(host_append_run_command_windows(command, sizeof(command), "C:\\Tools\\test.cmd",
                                            arguments, 0),
            "unsafe batch arguments should still build a guarded script");
    {
        static char decoded[HOST_MAX_COMMAND_LEN * 2];

        decode_ps_command(command, decoded, sizeof(decoded));
        require_contains(decoded, "$f='C:\\Tools\\test.cmd';");
        require_contains(decoded, "[IO.Path]::GetExtension($f)");
        require_contains(decoded, "Get-Command -Name $f -CommandType Application");
        require_contains(decoded, "if($e -match '^\\.(bat|cmd)$'){throw ");
        require_contains(decoded, "Start-Process -FilePath $f -ArgumentList "
                                  "'\"say \\\"hi\\\"\" 100%'");
    }
}

/*
 * The run arguments host-run builds for a batch file must keep cmd.exe
 * from seeing an operator: the old code used C runtime quoting, which
 * leaves & unquoted.
 */
static void test_windows_run_batch_arguments(void)
{
    char arguments[256];
    char crt[256];

    arguments[0] = '\0';
    crt[0] = '\0';
    require(host_append_cmd_word(arguments, sizeof(arguments), "x&calc.exe", 0) &&
            host_append_cmdline_arg(crt, sizeof(crt), "x&calc.exe", 0),
            "operator argument should build");
    require_string(arguments, "\"x&calc.exe\"", "cmd.exe words must quote an ampersand");
    require_string(crt, "x&calc.exe", "C runtime quoting leaves the ampersand bare");

    arguments[0] = '\0';
    require(host_append_cmd_word(arguments, sizeof(arguments), "a|b<c>d^e(f)", 0),
            "all cmd.exe operators should be quotable");
    require_string(arguments, "\"a|b<c>d^e(f)\"", "cmd.exe operators need quotes");

    arguments[0] = '\0';
    require(!host_append_cmd_word(arguments, sizeof(arguments), "100%", 0) &&
            !host_append_cmd_word(arguments, sizeof(arguments), "say \"hi\"", 0) &&
            !host_append_cmd_word(arguments, sizeof(arguments), "a\nb", 0),
            "percent signs, quotes and line breaks cannot be passed to a batch file");
    require_string(arguments, "", "a rejected word leaves the buffer unchanged");

    strcpy(arguments, "ab");
    require(!host_append_cmd_word(arguments, 6, "c d e f", 1),
            "a word that does not fit should fail");
    require_string(arguments, "ab", "a word that does not fit leaves the buffer unchanged");
}

static void test_posix_run_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), "ls", " -l 'My Files'"),
            "posix run command should build");
    require_contains(command, "p=ls; case $p in */*) [ -f \"$p\" ] && [ -x \"$p\" ];;");
    require_contains(command, "*) command -v -- \"$p\" >/dev/null 2>&1;; esac || ");
    require_contains(command, "Command not found on host: %s");
    require_contains(command, "exit 127");
    require_contains(command, "( \"$p\" -l 'My Files' >/dev/null 2>&1 </dev/null & ); exit 0");
    require_shell_syntax(command);

    command[0] = '\0';
    require(!host_append_run_command(command, 20, "ls", ""),
            "small posix run buffer should fail");
}

/* Run script under sh and capture its output and exit status */
static int run_script(const char *script, char *output, size_t output_size)
{
    char wrapped[HOST_MAX_COMMAND_LEN * 2];
    FILE *pipe;
    size_t len;
    int status;

    wrapped[0] = '\0';
    require(host_append_literal(wrapped, sizeof(wrapped), "sh -c ") &&
            host_append_shell_arg(wrapped, sizeof(wrapped), script, 0),
            "script wrapper should fit");
    pipe = popen(wrapped, "r");
    require(pipe != NULL, "script should start");
    len = fread(output, 1, output_size - 1, pipe);
    output[len] = '\0';
    status = pclose(pipe);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int wait_for_file(const char *path, char *contents, size_t size)
{
    for (int i = 0; i < 50; i++) {
        FILE *f = fopen(path, "r");

        if (f != NULL) {
            size_t len = fread(contents, 1, size - 1, f);

            fclose(f);
            if (len > 0 && contents[len - 1] == '\n') {
                contents[len] = '\0';
                return 1;
            }
        }
        {
            struct timespec pause = { 0, 100000000L };

            nanosleep(&pause, NULL);
        }
    }
    return 0;
}

static void test_posix_run_behavior(void)
{
    char dir[] = "/tmp/host-tools-run.XXXXXX";
    char out_path[256];
    char program[512];
    char arguments[512];
    char command[HOST_MAX_COMMAND_LEN];
    char output[256];
    char contents[256];
    struct timespec start;
    struct timespec end;
    double elapsed;

    require(mkdtemp(dir) != NULL, "temp dir should be created");
    snprintf(out_path, sizeof(out_path), "%s/out", dir);

    /* a missing program is reported, not silently accepted (it used to return 0) */
    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), "definitely-not-installed-xyz", ""),
            "missing program command should build");
    require(run_script(command, output, sizeof(output)) == 127,
            "a missing program should exit 127");
    require_contains(output, "Command not found on host: definitely-not-installed-xyz");

    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), "/nonexistent/dir/prog", ""),
            "missing path command should build");
    require(run_script(command, output, sizeof(output)) == 127,
            "a missing program path should exit 127");

    /* dash's command -v accepts any existing path: a directory or plain file must not pass */
    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), dir, ""),
            "directory command should build");
    require(run_script(command, output, sizeof(output)) == 127,
            "a directory is not a program");
    snprintf(out_path, sizeof(out_path), "%s/plain", dir);
    {
        FILE *plain = fopen(out_path, "w");

        require(plain != NULL, "plain file should be created");
        fputs("data\n", plain);
        fclose(plain);
    }
    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), out_path, ""),
            "plain file command should build");
    require(run_script(command, output, sizeof(output)) == 127,
            "a file without the executable bit is not a program");
    snprintf(out_path, sizeof(out_path), "%s/out", dir);

    /* the program gets its arguments verbatim and runs detached from the script */
    program[0] = '\0';
    arguments[0] = '\0';
    require(host_append_shell_arg(program, sizeof(program), "/bin/sh", 0) &&
            host_append_shell_arg(arguments, sizeof(arguments), "-c", 1) &&
            host_append_shell_arg(arguments, sizeof(arguments),
                                  "printf '%s|%s|%s\\n' \"$0\" \"$1\" \"$2\" > \"$3\"; sleep 5", 1) &&
            host_append_shell_arg(arguments, sizeof(arguments), "a b", 1) &&
            host_append_shell_arg(arguments, sizeof(arguments), "it's $HOME", 1) &&
            host_append_shell_arg(arguments, sizeof(arguments), "", 1) &&
            host_append_shell_arg(arguments, sizeof(arguments), out_path, 1),
            "program words should build");
    command[0] = '\0';
    require(host_append_run_command(command, sizeof(command), program, arguments),
            "detached run command should build");

    clock_gettime(CLOCK_MONOTONIC, &start);
    require(run_script(command, output, sizeof(output)) == 0,
            "a program that exists should be started with exit 0");
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = (double)(end.tv_sec - start.tv_sec) + (double)(end.tv_nsec - start.tv_nsec) / 1e9;
    require(elapsed < 3.0,
            "the script must end when the program is started, not when it quits");
    require_string(output, "", "a started program must not print into the session");
    require(wait_for_file(out_path, contents, sizeof(contents)),
            "the started program should run");
    require_string(contents, "a b|it's $HOME|\n", "arguments should reach the program as written");
}

static void test_windows_shell_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char *none[] = { "host-shell" };
    char *single[] = { "host-shell", "dir \"C:\\My Files\" & echo done" };
    char *several[] = { "host-shell", "type", "C:\\My Files\\a b.txt", "", "x&y", "plain-1.txt" };
    char *quoted[] = { "host-shell", "echo", "say \"hi\"" };
    char *newline[] = { "host-shell", "echo", "a\nb" };
    char *percent[] = { "host-shell", "type", "C:\\data\\%USERNAME%.txt" };
    char *trailing[] = { "host-shell", "app.exe", "C:\\My Files\\", "next", "C:\\plain\\" };
    char parsed[8][64];
    char small[16];

    command[0] = '\0';
    require(host_append_shell_windows_command(command, sizeof(command), 1, none),
            "windows shell command without arguments should build");
    require_string(command, "", "no arguments should leave an interactive cmd.exe session");

    command[0] = '\0';
    require(host_append_shell_windows_command(command, sizeof(command), 2, single),
            "windows shell command with one argument should build");
    require_string(command, "dir \"C:\\My Files\" & echo done",
                   "a single argument should be used as written");

    command[0] = '\0';
    require(host_append_shell_windows_command(command, sizeof(command), 6, several),
            "windows shell command with several arguments should build");
    require_string(command, "type \"C:\\My Files\\a b.txt\" \"\" \"x&y\" plain-1.txt",
                   "several arguments should be joined as cmd.exe words");

    /* a quoted word ending in backslashes must not swallow its closing quote */
    command[0] = '\0';
    require(host_append_shell_windows_command(command, sizeof(command), 5, trailing),
            "windows shell command with trailing backslashes should build");
    require_string(command, "app.exe \"C:\\My Files\\\\\" next C:\\plain\\",
                   "trailing backslashes of a quoted word should be doubled");
    require(split_cmdline(command, parsed, 8) == 4 &&
            strcmp(parsed[1], "C:\\My Files\\") == 0 && strcmp(parsed[2], "next") == 0 &&
            strcmp(parsed[3], "C:\\plain\\") == 0,
            "a C runtime parser should read the quoted directory and the next word separately");
    command[0] = '\0';
    require(!host_append_shell_windows_command(command, sizeof(command), 3, quoted),
            "argument with a quote should be rejected");
    command[0] = '\0';
    require(!host_append_shell_windows_command(command, sizeof(command), 3, newline),
            "argument with a line break should be rejected");
    command[0] = '\0';
    require(!host_append_shell_windows_command(command, sizeof(command), 3, percent),
            "argument with a percent sign should be rejected");
    require(!host_cmd_word_valid("a\"b") && !host_cmd_word_valid("a\rb") &&
            !host_cmd_word_valid("%PATH%") && host_cmd_word_valid("a b&c"),
            "cmd word validation should reject only quotes, percent signs and line breaks");

    small[0] = '\0';
    require(!host_append_shell_windows_command(small, sizeof(small), 6, several),
            "windows shell command over the buffer size should fail");
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

    command[0] = '\0';
    require(!host_append_env_set_command(command, sizeof(command), "FOO", "Text"),
            "small env set command buffer should fail");
}

int main(void)
{
    test_reveal_command();
    test_reveal_command_without_uri();
    test_notify_command();
    test_clip_copy_command();
    test_clip_copy_command_multiline();
    test_clip_paste_command();
    test_info_command();
    test_env_name_validation();
    test_env_get_command();
    test_env_get_command_output();
    test_env_set_command();
    test_env_unset_command();
    test_env_set_behavior();
    test_env_list_command();
    test_shell_login_interactive_command();
    test_shell_login_explicit_command();
    test_shell_login_command_limit();
    test_ps_quoting();
    test_ps_encoding();
    test_windows_clip_commands();
    test_windows_reveal_command();
    test_windows_info_command();
    test_windows_env_commands();
    test_cmdline_arg_quoting();
    test_windows_notify_command();
    test_windows_edit_command();
    test_windows_multiview_command();
    test_windows_run_command();
    test_windows_run_batch_arguments();
    test_posix_run_command();
    test_posix_run_behavior();
    test_windows_shell_command();
    test_small_buffer_failures();
    return 0;
}
