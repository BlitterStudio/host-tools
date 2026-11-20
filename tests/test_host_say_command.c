/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "host_base64.h"
#include "host_say_command.h"

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

static void require_ps_script(const char *command, const char *script)
{
    static const char prefix[] = "powershell -NoProfile -EncodedCommand ";
    struct host_base64_state st;
    unsigned char decoded[4096];
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

/* ---- POSIX script, run against stub speech programs ---- */

static char workdir[] = "/tmp/host-say-test-XXXXXX";

static void path_in(char *dest, size_t size, const char *name)
{
    require(snprintf(dest, size, "%s/%s", workdir, name) < (int)size, "path should fit");
}

static void write_file(const char *name, const char *content, int executable)
{
    char path[512];
    FILE *f;

    path_in(path, sizeof(path), name);
    f = fopen(path, "wb");
    require(f != NULL, "test file should open");
    require(fwrite(content, 1, strlen(content), f) == strlen(content), "test file should write");
    require(fclose(f) == 0, "test file should close");
    if (executable) {
        require(chmod(path, 0755) == 0, "test file should become executable");
    }
}

static void remove_stubs(void)
{
    char command[1024];

    /* the stubs live in a directory that holds nothing else of value */
    snprintf(command, sizeof(command),
             "rm -f '%s'/say '%s'/spd-say '%s'/espeak-ng '%s'/espeak '%s'/args '%s'/stdin '%s'/spd-say.pid '%s'/spd-say.term '%s'/spd-say.cancel",
             workdir, workdir, workdir, workdir, workdir, workdir, workdir, workdir, workdir);
    require(system(command) == 0, "stubs should be removed");
}

/* A stub that records its arguments, one per line, and its standard input */
static void write_stub(const char *name)
{
    char script[512];

    snprintf(script, sizeof(script),
             "#!/bin/sh\n"
             "{ echo %s; for a in \"$@\"; do printf '[%%s]\\n' \"$a\"; done; } > '%s/args'\n"
             "if [ \"%s\" = say ]; then IFS= read -r l; printf %%s \"$l\" > '%s/stdin'; fi\n",
             name, workdir, name, workdir);
    write_file(name, script, 1);
}

static void read_file(const char *name, char *buffer, size_t size)
{
    char path[512];
    FILE *f;
    size_t n;

    path_in(path, sizeof(path), name);
    f = fopen(path, "rb");
    require(f != NULL, "recorded file should exist");
    n = fread(buffer, 1, size - 1, f);
    buffer[n] = '\0';
    fclose(f);
}

/* Run the command with only the stub directory (and iconv, when present) on PATH */
static int run_posix(const char *command)
{
    pid_t pid;
    int status;

    pid = fork();
    require(pid >= 0, "fork should work");
    if (pid == 0) {
        setenv("PATH", workdir, 1);
        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(126);
    }
    require(waitpid(pid, &status, 0) == pid, "waitpid should work");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static int have_iconv;

static void link_tool(const char *name)
{
    char command[1024];
    int rc;

    snprintf(command, sizeof(command), "ln -s \"$(command -v %s)\" '%s'/%s 2>/dev/null", name, workdir, name);
    rc = system(command);
    if (strcmp(name, "iconv") == 0) {
        have_iconv = rc == 0;
    } else {
        require(rc == 0, "tool should be linked into the stub directory");
    }
}

static void test_say_command_is_valid_shell(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char syntax[HOST_MAX_COMMAND_LEN * 2];

    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "it's -a \"test\" $(rm -rf /)"),
            "say command should build");
    syntax[0] = '\0';
    require(host_append_literal(syntax, sizeof(syntax), "sh -n -c ") &&
            host_append_shell_arg(syntax, sizeof(syntax), command, 0),
            "syntax command should fit");
    require(system(syntax) == 0, "say command should parse");
}

static void test_say_prefers_say_and_passes_text_on_stdin(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char recorded[512];

    remove_stubs();
    write_stub("say");
    write_stub("spd-say");
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "-v Bad voice"), "command should build");
    require(run_posix(command) == 0, "say stub should succeed");
    read_file("args", recorded, sizeof(recorded));
    require_string(recorded, "say\n[-f]\n[-]\n", "say should read the text from standard input");
    read_file("stdin", recorded, sizeof(recorded));
    require_string(recorded, "-v Bad voice", "say should receive the text, not options");
}

static void test_spd_say_waits_and_ends_options(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char recorded[512];

    remove_stubs();
    write_stub("spd-say");
    write_stub("espeak-ng");
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "--help it's \"$HOME\" `id`"),
            "command should build");
    require(run_posix(command) == 0, "spd-say stub should succeed");
    read_file("args", recorded, sizeof(recorded));
    require_string(recorded, "spd-say\n[-w]\n[--]\n[--help it's \"$HOME\" `id`]\n",
                   "spd-say should wait and get the text after --");
}

static void test_espeak_fallbacks(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char recorded[512];

    remove_stubs();
    write_stub("espeak-ng");
    write_stub("espeak");
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "-s 400 hello"), "command should build");
    require(run_posix(command) == 0, "espeak-ng stub should succeed");
    read_file("args", recorded, sizeof(recorded));
    require_string(recorded, "espeak-ng\n[--]\n[-s 400 hello]\n", "espeak-ng should get the text after --");

    remove_stubs();
    write_stub("espeak");
    require(run_posix(command) == 0, "espeak stub should succeed");
    read_file("args", recorded, sizeof(recorded));
    require_string(recorded, "espeak\n[--]\n[-s 400 hello]\n", "espeak should get the text after --");
}

static void test_no_backend_exits_127(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    remove_stubs();
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "hello"), "command should build");
    require(run_posix(command) == 127, "missing speech programs should exit 127");
}

static void test_exit_status_is_passed_on(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    remove_stubs();
    write_file("spd-say", "#!/bin/sh\nexit 3\n", 1);
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "hello"), "command should build");
    require(run_posix(command) == 3, "the speech program's status should be the script's status");
}

static void test_latin1_text_becomes_utf8(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    char recorded[512];

    if (!have_iconv) {
        printf("skipping Latin-1 conversion test: no iconv\n");
        return;
    }
    remove_stubs();
    write_stub("spd-say");
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "caf\xe9 na\xefve"), "command should build");
    require(run_posix(command) == 0, "spd-say stub should succeed");
    read_file("args", recorded, sizeof(recorded));
    require_string(recorded, "spd-say\n[-w]\n[--]\n[caf\xc3\xa9 na\xc3\xafve]\n",
                   "ISO-8859-1 text should reach the program as UTF-8");
}

static void wait_for_file(const char *name)
{
    char path[512];

    path_in(path, sizeof(path), name);
    for (int i = 0; i < 100; i++) {
        struct timespec pause = { 0, 50 * 1000 * 1000 };

        if (access(path, F_OK) == 0) {
            return;
        }
        nanosleep(&pause, NULL);
    }
    require(0, "an expected test file did not appear");
}

/*
 * Closing a session sends SIGTERM to the shell. The stub spd-say records
 * the SIGTERM it receives in a marker file, which holds whether or not
 * the stopped process is reaped, and a second "spd-say -C" call marks
 * the cancel request to the speech server.
 */
static void test_session_close_stops_speech(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    pid_t shell;
    int status;

    remove_stubs();
    write_file("spd-say",
               "#!/bin/sh\n"
               "if [ \"$1\" = -C ]; then : > \"$0.cancel\"; exit 0; fi\n"
               "trap ': > \"$0.term\"; exit 0' TERM\n"
               "echo $$ > \"$0.pid\"\n"
               "while :; do sleep 1; done\n", 1);
    command[0] = '\0';
    require(host_append_say_command(command, sizeof(command), "a long sentence"), "command should build");

    shell = fork();
    require(shell >= 0, "fork should work");
    if (shell == 0) {
        setenv("PATH", workdir, 1);
        freopen("/dev/null", "r", stdin);
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        _exit(126);
    }

    wait_for_file("spd-say.pid");

    /* what Amiberry does when the guest closes a session */
    require(kill(shell, SIGTERM) == 0, "SIGTERM should reach the shell");
    require(waitpid(shell, &status, 0) == shell, "the shell should end");
    require(WIFEXITED(status) && WEXITSTATUS(status) == 1, "the trap should end the shell with status 1");

    wait_for_file("spd-say.term");
    wait_for_file("spd-say.cancel");
}

/* ---- Windows script ---- */

static void test_windows_say_command(void)
{
    char command[HOST_MAX_COMMAND_LEN];

    command[0] = '\0';
    require(host_append_say_command_windows(command, sizeof(command), "it's -a \"test\""),
            "windows say command should build");
    require_ps_script(command,
                      "$ErrorActionPreference='Stop';"
                      "Add-Type -AssemblyName System.Speech;"
                      "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;"
                      "$b=New-Object byte[] 1;"
                      "$r=[Console]::OpenStandardInput().ReadAsync($b,0,1);"
                      "$p=$s.SpeakAsync('it''s -a \"test\"');"
                      "while(-not $p.IsCompleted -and -not $r.IsCompleted){Start-Sleep -Milliseconds 50};"
                      "if(-not $p.IsCompleted){$s.SpeakAsyncCancelAll()};"
                      "$s.Dispose()");
}

static void test_windows_say_command_limit(void)
{
    char command[HOST_MAX_COMMAND_LEN];
    static char text[2048];

    /* the limit quoted in host-say's error message */
    memset(text, 'a', 1100);
    text[1100] = '\0';
    command[0] = '\0';
    require(host_append_say_command_windows(command, sizeof(command), text),
            "windows say command should fit 1100 characters");
    memset(text, 'a', sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';
    command[0] = '\0';
    require(!host_append_say_command_windows(command, sizeof(command), text),
            "windows say command over the encoded size limit should fail");
}

static void test_small_buffer_failures(void)
{
    char command[32];

    command[0] = '\0';
    require(!host_append_say_command(command, sizeof(command), "Text"),
            "small say command buffer should fail");
    command[0] = '\0';
    require(!host_append_say_command_windows(command, sizeof(command), "Text"),
            "small windows say command buffer should fail");
}

int main(void)
{
    require(mkdtemp(workdir) != NULL, "test directory should be created");
    link_tool("sleep");
    link_tool("iconv");

    test_say_command_is_valid_shell();
    test_say_prefers_say_and_passes_text_on_stdin();
    test_spd_say_waits_and_ends_options();
    test_espeak_fallbacks();
    test_no_backend_exits_127();
    test_exit_status_is_passed_on();
    test_latin1_text_becomes_utf8();
    test_session_close_stops_speech();
    test_windows_say_command();
    test_windows_say_command_limit();
    test_small_buffer_failures();

    {
        char command[1024];
        snprintf(command, sizeof(command), "rm -rf '%s'", workdir);
        if (system(command) != 0) {
            fprintf(stderr, "could not remove %s\n", workdir);
        }
    }
    return 0;
}
