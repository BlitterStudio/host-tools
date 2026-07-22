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
#include <unistd.h>
#include "host_base64.h"
#include "host_clip_command.h"
#include "host_env_command.h"
#include "host_info_command.h"
#include "host_notify_command.h"
#include "host_powershell.h"
#include "host_reveal_command.h"
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
    require_contains(command, "notify-send \"$t\" \"$m\"");
    require_contains(command, "osascript -e 'on run argv'");
    require_contains(command, "'end run' \"$t\" \"$m\"; else exit 127; fi");
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

    require_contains(command, "umask 077; f=\"${HOME:?}/.host-tools-env\"");
    require_contains(command, "if [ \"${FOO+x}\" = x ]; then printf %s \"$FOO\"");
    require_contains(command, ". \"$f\"");
    require_contains(command, "else exit 1; fi");
    require_shell_syntax(command);
}

static void test_env_set_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_set_command(command, sizeof(command),
                                        "FOO", "a b's $HOME"),
            "env set command should build");

    require_contains(command, "umask 077; f=\"${HOME:?}/.host-tools-env\"");
    require_contains(command, "t=$(mktemp \"${f}.XXXXXX\")");
    require_contains(command, "trap 'rm -f \"$t\"' 0 1 2 15");
    require_contains(command, "grep -v '^export FOO=' \"$f\"");
    require_contains(command, "printf '%s\\n' 'export FOO='\\''a b'\\''\\'\\'''\\''s $HOME'\\'''");
    require_contains(command, "mv \"$t\" \"$f\"");
    require_contains(command, "chmod 600 \"$t\"");
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

    require_contains(command, "umask 077; f=\"${HOME:?}/.host-tools-env\"");
    require_contains(command, "t=$(mktemp \"${f}.XXXXXX\")");
    require_contains(command, "grep -v '^export FOO=' \"$f\" > \"$t\"");
    require_contains(command, "mv \"$t\" \"$f\"");
    require_contains(command, "chmod 600 \"$t\"");
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
    require(rmdir(temp_dir) == 0, "env behavior temp directory should be removed");
}

static void test_env_list_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_env_list_command(command, sizeof(command)),
            "env list command should build");

    require_contains(command, "f=\"${HOME:?}/.host-tools-env\"");
    require_contains(command, "env");
    require_contains(command, ". \"$f\"");
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

static void require_ps_script(const char *command, const char *script)
{
    static const char prefix[] = "powershell -NoProfile -EncodedCommand ";
    struct host_base64_state st;
    unsigned char decoded[2048];
    long n;
    size_t script_len = strlen(script);

    require(strncmp(command, prefix, sizeof(prefix) - 1) == 0,
            "encoded command should start with the powershell prefix");

    host_base64_init(&st);
    n = host_base64_feed(&st, command + sizeof(prefix) - 1,
                         (long)strlen(command + sizeof(prefix) - 1), decoded);
    require(n == (long)(script_len * 2), "decoded UTF-16 length should match script");
    for (size_t i = 0; i < script_len; i++) {
        require(decoded[i * 2] == (unsigned char)script[i] && decoded[i * 2 + 1] == 0,
                "decoded UTF-16 bytes should spell the script");
    }
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
    require(strcmp(command, "explorer /select,\"C:\\My Files\\disk.adf\" & exit 0") == 0,
            "windows reveal command should select the file and mask explorer's exit code");
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
                      "if($null -eq $v){exit 1};[Console]::Out.Write($v)");

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
    test_env_set_command();
    test_env_unset_command();
    test_env_set_behavior();
    test_env_list_command();
    test_shell_login_interactive_command();
    test_shell_login_explicit_command();
    test_shell_login_command_limit();
    test_ps_quoting();
    test_windows_clip_commands();
    test_windows_reveal_command();
    test_windows_info_command();
    test_windows_env_commands();
    test_small_buffer_failures();
    return 0;
}
