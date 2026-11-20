/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Runs the generated POSIX host scripts for real, with a private PATH
 * that holds symlinks to the few system tools they need plus fake
 * curl, wget, notify-send, osascript, xdg-mime, gtk-launch and
 * xdg-open programs that record how they were called. HOST_TEST_SHELL
 * picks the shell that runs the scripts (default /bin/sh).
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "host_clip_command.h"
#include "host_download_command.h"
#include "host_edit_command.h"
#include "host_env_command.h"
#include "host_notify_command.h"

#define BIG (HOST_MAX_COMMAND_LEN * 4)

static char root[64];
static const char *shell = "/bin/sh";
static int dir_counter;

static void fail(const char *message, const char *detail)
{
    fprintf(stderr, "%s\n", message);
    if (detail != NULL) {
        fprintf(stderr, "%s\n", detail);
    }
    exit(1);
}

static void require(int condition, const char *message)
{
    if (!condition) {
        fail(message, NULL);
    }
}

static void require_text(const char *actual, const char *expected, const char *message)
{
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s\nexpected: [%s]\nactual:   [%s]\n", message, expected, actual);
        exit(1);
    }
}

static void require_contains(const char *value, const char *needle, const char *message)
{
    if (strstr(value, needle) == NULL) {
        fprintf(stderr, "%s\nmissing: [%s]\nin: [%s]\n", message, needle, value);
        exit(1);
    }
}

static void require_not_contains(const char *value, const char *needle, const char *message)
{
    if (strstr(value, needle) != NULL) {
        fprintf(stderr, "%s\nunexpected: [%s]\nin: [%s]\n", message, needle, value);
        exit(1);
    }
}

static void sh(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

static void sh(const char *fmt, ...)
{
    char command[BIG];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(command, sizeof(command), fmt, ap);
    va_end(ap);
    require(system(command) == 0, command);
}

/* A fresh directory below the test root. */
static const char *make_dir(char *path, size_t size, const char *label)
{
    snprintf(path, size, "%s/%s%d", root, label, ++dir_counter);
    require(mkdir(path, 0700) == 0, "test directory should be created");
    return path;
}

static void write_file(const char *path, const char *content, int executable)
{
    FILE *f = fopen(path, "w");

    require(f != NULL, "test file should open");
    require(fputs(content, f) >= 0, "test file should write");
    require(fclose(f) == 0, "test file should close");
    if (executable) {
        require(chmod(path, 0700) == 0, "test file should become executable");
    }
}

static const char *read_file(const char *path)
{
    static char buffer[16384];
    FILE *f = fopen(path, "r");
    size_t n;

    buffer[0] = '\0';
    if (f == NULL) {
        return buffer;
    }
    n = fread(buffer, 1, sizeof(buffer) - 1, f);
    buffer[n] = '\0';
    fclose(f);
    return buffer;
}

/* A PATH directory holding only the system tools the scripts use. */
static void make_bin(char *bin, size_t size, int with_flock)
{
    make_dir(bin, size, "bin");
    sh("for t in sh uname grep mktemp mv rm rmdir chmod mkdir sleep sort env cat tr awk wc "
       "base64 kill iconv dirname%s; do "
       "for d in $(echo \"$PATH\" | tr : ' '); do "
       "if [ -x \"$d/$t\" ] && [ ! -d \"$d/$t\" ]; then ln -s \"$d/$t\" '%s'/$t; break; fi; done; done; true",
       with_flock ? " flock" : "", bin);
}

static void fake_tool(const char *bin, const char *name, const char *script)
{
    char path[256];

    snprintf(path, sizeof(path), "%s/%s", bin, name);
    write_file(path, script, 1);
}

/*
 * Run a generated command like Amiberry does, with "sh -c <command>"
 * and a PATH made of the given bin directory only. Standard output
 * goes to a file; the exit status is returned.
 */
static int run_command(const char *bin, const char *home, const char *extra_env,
                       const char *command, const char *out_path)
{
    static char line[BIG];
    int status;

    line[0] = '\0';
    require(host_append_literal(line, sizeof(line), "env -i PATH='") &&
                host_append_literal(line, sizeof(line), bin) &&
                host_append_literal(line, sizeof(line), "' HOME='") &&
                host_append_literal(line, sizeof(line), home) &&
                host_append_literal(line, sizeof(line), "' TMPDIR='") &&
                host_append_literal(line, sizeof(line), home) &&
                host_append_literal(line, sizeof(line), "' ") &&
                host_append_literal(line, sizeof(line), extra_env) &&
                host_append_literal(line, sizeof(line), " ") &&
                host_append_literal(line, sizeof(line), shell) &&
                host_append_literal(line, sizeof(line), " -c ") &&
                host_append_shell_arg(line, sizeof(line), command, 0) &&
                host_append_literal(line, sizeof(line), " > '") &&
                host_append_literal(line, sizeof(line), out_path) &&
                host_append_literal(line, sizeof(line), "' 2>/dev/null"),
            "run command should fit");
    status = system(line);
    require(status != -1 && (status & 0x7f) == 0, "scripts should exit normally");
    return (status >> 8) & 0xff;
}

static int exists(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0;
}

static int has_flock(void)
{
    return system("command -v flock >/dev/null 2>&1") == 0;
}

/* --- host-env ---------------------------------------------------- */

/*
 * "host-env get f" used to print the helper's own file path, because
 * the script kept its scratch state in a variable called f.
 */
static void test_env_get_finds_user_variable_named_like_scratch_state(void)
{
    char bin[200], home[200], out[260], cmd_file[260];
    char command[HOST_MAX_COMMAND_LEN];
    static const char *names[] = { "f", "t", "n", "s", "p", "m" };

    make_bin(bin, sizeof(bin), 0);
    make_dir(home, sizeof(home), "home");
    snprintf(out, sizeof(out), "%s/out", home);
    snprintf(cmd_file, sizeof(cmd_file), "%s/.host-tools-env", home);
    write_file(cmd_file,
               "export f='persisted f'\nexport t='persisted t'\nexport n='persisted n'\n"
               "export s='persisted s'\nexport p='persisted p'\nexport m='persisted m'\n", 0);

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char expected[64];

        command[0] = '\0';
        require(host_append_env_get_command(command, sizeof(command), names[i]),
                "env get should build");
        require(run_command(bin, home, "", command, out) == 0, "get should find the variable");
        snprintf(expected, sizeof(expected), "persisted %s\n", names[i]);
        require_text(read_file(out), expected, "get should print the stored value");
    }

    /* unset everywhere: exit status 1 and no output instead of a path */
    unlink(cmd_file);
    command[0] = '\0';
    require(host_append_env_get_command(command, sizeof(command), "f"), "env get should build");
    require(run_command(bin, home, "", command, out) == 1, "missing variable should fail");
    require_text(read_file(out), "", "missing variable should print nothing");

    /* the process environment wins, as before */
    require(run_command(bin, home, "f=from-environment", command, out) == 0,
            "environment variable should be found");
    require_text(read_file(out), "from-environment\n", "environment value should be printed");
}

static void write_env_command(const char *path, int index, int unset)
{
    char name[32];
    char value[32];
    char command[HOST_MAX_COMMAND_LEN];

    snprintf(name, sizeof(name), "VAR%d", index);
    snprintf(value, sizeof(value), "value %d", index);
    command[0] = '\0';
    if (unset) {
        require(host_append_env_unset_command(command, sizeof(command), name), "unset should build");
    } else {
        require(host_append_env_set_command(command, sizeof(command), name, value),
                "set should build");
    }
    write_file(path, command, 0);
}

/* Without a lock two parallel sets read the same file and one change is lost. */
static void env_parallel_updates(int with_flock)
{
    char bin[200], home[200], script[260], cmd[260], file[260], leftovers[260];
    const char *contents;

    make_bin(bin, sizeof(bin), with_flock);
    make_dir(home, sizeof(home), "home");
    snprintf(script, sizeof(script), "%s/parallel.sh", home);
    snprintf(file, sizeof(file), "%s/.host-tools-env", home);

    /* twelve parallel sets */
    {
        char body[BIG] = "";

        for (int i = 1; i <= 12; i++) {
            snprintf(cmd, sizeof(cmd), "%s/set%d.sh", home, i);
            write_env_command(cmd, i, 0);
            snprintf(body + strlen(body), sizeof(body) - strlen(body),
                     "env -i PATH='%s' HOME='%s' %s -c \"$(cat '%s')\" &\n", bin, home, shell, cmd);
        }
        snprintf(body + strlen(body), sizeof(body) - strlen(body), "wait\n");
        write_file(script, body, 0);
    }
    sh("%s '%s'", shell, script);
    contents = read_file(file);
    for (int i = 1; i <= 12; i++) {
        char line[64];

        snprintf(line, sizeof(line), "export VAR%d='value %d'\n", i, i);
        require_contains(contents, line, "every parallel set should survive");
    }

    /* six parallel unsets and six parallel sets of other names */
    {
        char body[BIG] = "";

        for (int i = 1; i <= 6; i++) {
            snprintf(cmd, sizeof(cmd), "%s/unset%d.sh", home, i);
            write_env_command(cmd, i, 1);
            snprintf(body + strlen(body), sizeof(body) - strlen(body),
                     "env -i PATH='%s' HOME='%s' %s -c \"$(cat '%s')\" &\n", bin, home, shell, cmd);
            snprintf(cmd, sizeof(cmd), "%s/set%d.sh", home, 12 + i);
            write_env_command(cmd, 12 + i, 0);
            snprintf(body + strlen(body), sizeof(body) - strlen(body),
                     "env -i PATH='%s' HOME='%s' %s -c \"$(cat '%s')\" &\n", bin, home, shell, cmd);
        }
        snprintf(body + strlen(body), sizeof(body) - strlen(body), "wait\n");
        write_file(script, body, 0);
    }
    sh("%s '%s'", shell, script);
    contents = read_file(file);
    for (int i = 1; i <= 18; i++) {
        char line[64];

        snprintf(line, sizeof(line), "export VAR%d=", i);
        if (i <= 6) {
            require_not_contains(contents, line, "unset should remove its variable");
        } else {
            require_contains(contents, line, "other variables should survive parallel unsets");
        }
    }

    /* no temporary files, no lock directory; flock leaves its lock file */
    snprintf(leftovers, sizeof(leftovers), "%s/.host-tools-env.lock.d", home);
    require(!exists(leftovers), "lock directory should be removed");
    {
        DIR *d = opendir(home);
        struct dirent *e;

        require(d != NULL, "home should open");
        while ((e = readdir(d)) != NULL) {
            require(strncmp(e->d_name, ".host-tools-env.", 16) != 0 ||
                        strcmp(e->d_name, ".host-tools-env.lock") == 0,
                    "no temporary env files should remain");
        }
        closedir(d);
    }
}

static void test_env_parallel_updates(void)
{
    env_parallel_updates(0);
    if (has_flock()) {
        env_parallel_updates(1);
    }
}

/*
 * A set that is stopped while it holds the lock (the fake grep is
 * slow) must clean up its lock and temporary file, and a later set
 * must go through.
 */
static void env_signal_cleanup(int with_flock)
{
    char bin[200], home[200], script[260], file[260], path[260], real_grep[256];
    char command[HOST_MAX_COMMAND_LEN];
    FILE *p;
    const char *contents;

    make_bin(bin, sizeof(bin), with_flock);
    make_dir(home, sizeof(home), "home");
    snprintf(file, sizeof(file), "%s/.host-tools-env", home);
    write_file(file, "export KEEP='kept'\n", 0);

    p = popen("command -v grep", "r");
    require(p != NULL && fgets(real_grep, sizeof(real_grep), p) != NULL, "grep should be found");
    pclose(p);
    real_grep[strcspn(real_grep, "\n")] = '\0';
    snprintf(path, sizeof(path), "%s/grep", bin);
    unlink(path);
    {
        char body[512];

        snprintf(body, sizeof(body), "#!/bin/sh\nsleep 2\nexec '%s' \"$@\"\n", real_grep);
        fake_tool(bin, "grep", body);
    }

    command[0] = '\0';
    require(host_append_env_set_command(command, sizeof(command), "NEW", "value"),
            "set should build");
    snprintf(path, sizeof(path), "%s/set.sh", home);
    write_file(path, command, 0);

    snprintf(script, sizeof(script), "%s/kill.sh", home);
    {
        char body[BIG];

        snprintf(body, sizeof(body),
                 "env -i PATH='%s' HOME='%s' %s -c \"$(cat '%s')\" &\n"
                 "pid=$!\nsleep 1\nkill -TERM $pid\nwait $pid\nexit $?\n",
                 bin, home, shell, path);
        write_file(script, body, 0);
    }
    snprintf(path, sizeof(path), "%s/status", home);
    sh("%s '%s' > '%s' 2>/dev/null; echo $? >> '%s'", shell, script, path, path);
    contents = read_file(path);
    require(strcmp(contents, "0\n") != 0, "a terminated set should not report success");

    snprintf(path, sizeof(path), "%s/.host-tools-env.lock.d", home);
    require(!exists(path), "terminated set should remove its lock directory");
    {
        DIR *d = opendir(home);
        struct dirent *e;

        require(d != NULL, "home should open");
        while ((e = readdir(d)) != NULL) {
            require(strncmp(e->d_name, ".host-tools-env.", 16) != 0 ||
                        strcmp(e->d_name, ".host-tools-env.lock") == 0,
                    "terminated set should remove its temporary file");
        }
        closedir(d);
    }
    require_text(read_file(file), "export KEEP='kept'\n",
                 "terminated set should leave the env file alone");

    /* the lock is free again */
    snprintf(path, sizeof(path), "%s/grep", bin);
    unlink(path);
    sh("ln -s '%s' '%s'", real_grep, path);
    snprintf(path, sizeof(path), "%s/out", home);
    require(run_command(bin, home, "", command, path) == 0, "a later set should succeed");
    require_contains(read_file(file), "export NEW='value'\n", "later set should be stored");
}

static void test_env_signal_cleanup(void)
{
    env_signal_cleanup(0);
    if (has_flock()) {
        env_signal_cleanup(1);
    }
}

/* A lock directory held by someone else makes set give up with a message, without damage. */
static void test_env_lock_timeout(void)
{
    char bin[200], home[200], path[260], out[260];
    char command[HOST_MAX_COMMAND_LEN];

    make_bin(bin, sizeof(bin), 0);
    make_dir(home, sizeof(home), "home");
    snprintf(path, sizeof(path), "%s/.host-tools-env.lock.d", home);
    require(mkdir(path, 0700) == 0, "lock directory should be created");
    snprintf(out, sizeof(out), "%s/out", home);
    command[0] = '\0';
    require(host_append_env_set_command(command, sizeof(command), "X", "1"), "set should build");
    require(run_command(bin, home, "", command, out) == 1, "set should fail while locked");
    require_contains(read_file(out), "Cannot lock the environment file", "lock failure should be explained");
    require(exists(path), "someone else's lock directory must not be removed");
    snprintf(path, sizeof(path), "%s/.host-tools-env", home);
    require(!exists(path), "a failed set must not create the env file");
}

/* --- host-download ----------------------------------------------- */

static const char fake_curl[] =
    "#!/bin/sh\n"
    "for a in \"$@\"; do printf '[%s]' \"$a\"; done >> \"$FAKE_LOG\"; echo >> \"$FAKE_LOG\"\n"
    "case \"$*\" in\n"
    "*-sIL*) [ -n \"$FAKE_HEAD\" ] && cat \"$FAKE_HEAD\"; exit ${FAKE_HEAD_RC:-0};;\n"
    "esac\n"
    "printf 'BODY'\n"
    "exit ${FAKE_GET_RC:-0}\n";

static void download_case(const char *bin, const char *home, const char *head,
                          const char *head_rc, const char *expected_out, int expected_rc)
{
    char command[HOST_MAX_COMMAND_LEN];
    char env[600];
    char head_path[260];
    char out[260];

    snprintf(head_path, sizeof(head_path), "%s/head.txt", home);
    write_file(head_path, head, 0);
    snprintf(env, sizeof(env), "FAKE_LOG='%s/curl.log' FAKE_HEAD='%s' FAKE_HEAD_RC=%s FAKE_GET_RC=%d",
             home, head_path, head_rc, expected_rc);
    snprintf(out, sizeof(out), "%s/out", home);
    command[0] = '\0';
    require(host_append_download_stream_command(command, sizeof(command),
                                                "http://example.test/dir/f{a,b}[1-2].bin?x=1"),
            "download command should build");
    require(run_command(bin, home, env, command, out) == expected_rc,
            "download exit status should be the transfer's");
    require_text(read_file(out), expected_out, "download should print exactly one size line, then the body");
}

static void test_download_metadata_line(void)
{
    char bin[200], home[200], log[260];
    const char *logged;
    char *line;
    int lines = 0;

    make_bin(bin, sizeof(bin), 0);
    fake_tool(bin, "curl", fake_curl);
    make_dir(home, sizeof(home), "home");

    /* normal HEAD */
    download_case(bin, home, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\nX: y\r\n\r\n", "0",
                  "4\nBODY", 0);
    /* size beyond 2^31 stays an integer */
    download_case(bin, home, "HTTP/1.1 200 OK\r\ncontent-length: 5120000000\r\n\r\n", "0",
                  "5120000000\nBODY", 0);
    /* HEAD refused (405) but GET works: one line, "0", then the body */
    download_case(bin, home, "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 12\r\n\r\n", "22",
                  "0\nBODY", 0);
    /* HEAD fails with no output at all */
    download_case(bin, home, "", "7", "0\nBODY", 0);
    /* redirect: only the final response counts */
    download_case(bin, home,
                  "HTTP/1.1 302 Found\r\nContent-Length: 162\r\n\r\nHTTP/2 200\r\nContent-Length: 4\r\n\r\n",
                  "0", "4\nBODY", 0);
    download_case(bin, home,
                  "HTTP/1.1 302 Found\r\nContent-Length: 162\r\n\r\nHTTP/1.1 200 OK\r\n"
                  "Transfer-Encoding: chunked\r\n\r\n",
                  "0", "0\nBODY", 0);
    /* an error page's length is not the file's length */
    download_case(bin, home, "HTTP/1.1 404 Not Found\r\nContent-Length: 9\r\n\r\n", "22",
                  "0\nBODY", 0);
    /* FTP: no status line, the length counts */
    download_case(bin, home, "Content-Length: 77\r\nAccept-ranges: bytes\r\n\r\n", "0",
                  "77\nBODY", 0);
    /* a failed GET still reports curl's exit status */
    download_case(bin, home, "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\n", "0", "4\nBODY", 22);

    snprintf(log, sizeof(log), "%s/curl.log", home);
    logged = read_file(log);
    for (const char *p = logged; *p; p++) {
        if (*p == '\n') {
            lines++;
        }
    }
    require(lines == 18, "each download should run curl twice");
    line = strtok((char *)logged, "\n");
    while (line != NULL) {
        require_contains(line, "[--globoff]", "every curl call should disable globbing");
        require_contains(line, "[http://example.test/dir/f{a,b}[1-2].bin?x=1]",
                         "the URL should be passed as one argument");
        line = strtok(NULL, "\n");
    }
}

static void test_download_wget_and_missing_tools(void)
{
    char bin[200], home[200], out[260], log[260];
    char command[HOST_MAX_COMMAND_LEN];
    char env[400];

    make_bin(bin, sizeof(bin), 0);
    make_dir(home, sizeof(home), "home");
    snprintf(out, sizeof(out), "%s/out", home);
    command[0] = '\0';
    require(host_append_download_stream_command(command, sizeof(command), "http://example.test/f"),
            "download command should build");

    require(run_command(bin, home, "", command, out) == 127, "no downloader should give 127");

    fake_tool(bin, "wget", "#!/bin/sh\necho \"$*\" >> \"$FAKE_LOG\"\nprintf 'WGET'\n");
    snprintf(log, sizeof(log), "%s/wget.log", home);
    snprintf(env, sizeof(env), "FAKE_LOG='%s'", log);
    require(run_command(bin, home, env, command, out) == 0, "wget fallback should work");
    require_text(read_file(out), "0\nWGET", "wget fallback should print one size line, then the body");
}

/* Legacy pty session: spaces while the host downloads, then size and base64. */
static void test_download_legacy_heartbeat(void)
{
    char bin[200], home[200], out[260], tmpdir[260];
    char command[HOST_MAX_COMMAND_LEN];
    const char *result;
    size_t spaces;

    make_bin(bin, sizeof(bin), 0);
    fake_tool(bin, "curl",
              "#!/bin/sh\n"
              "out=; while [ $# -gt 0 ]; do [ \"$1\" = -o ] && out=$2; shift; done\n"
              "sleep 3\n"
              "printf 'abc' > \"$out\"\n"
              "exit ${FAKE_GET_RC:-0}\n");
    make_dir(home, sizeof(home), "home");
    snprintf(out, sizeof(out), "%s/out", home);
    snprintf(tmpdir, sizeof(tmpdir), "%s", home);
    command[0] = '\0';
    require(host_append_download_b64_command(command, sizeof(command), "http://example.test/f"),
            "legacy download command should build");

    require(run_command(bin, home, "", command, out) == 0, "legacy download should succeed");
    result = read_file(out);
    spaces = strspn(result, " ");
    require(spaces >= 2, "the host should print a space now and then while it downloads");
    require_text(result + spaces, "3\nYWJj\n", "legacy download should print the size, then base64");
    /* only out and the leftovers of the other runs remain: no mktemp files */
    {
        DIR *d = opendir(home);
        struct dirent *e;

        require(d != NULL, "home should open");
        while ((e = readdir(d)) != NULL) {
            require(strncmp(e->d_name, "tmp.", 4) != 0, "temporary download should be removed");
        }
        closedir(d);
    }

    require(run_command(bin, home, "FAKE_GET_RC=22", command, out) == 22,
            "failed legacy download should exit 22");
    result = read_file(out);
    require_text(result + strspn(result, " "), "", "failed legacy download should print no size or body");
    {
        DIR *d = opendir(home);
        struct dirent *e;

        require(d != NULL, "home should open");
        while ((e = readdir(d)) != NULL) {
            require(strncmp(e->d_name, "tmp.", 4) != 0, "temporary file should be removed after failure");
        }
        closedir(d);
    }
}

/* --- host-notify ------------------------------------------------- */

static void test_notify_arguments_are_not_options(void)
{
    char bin[200], home[200], out[260], log[260];
    char command[HOST_MAX_COMMAND_LEN];
    char env[400];

    make_dir(home, sizeof(home), "home");
    snprintf(out, sizeof(out), "%s/out", home);
    snprintf(log, sizeof(log), "%s/args.log", home);
    snprintf(env, sizeof(env), "FAKE_LOG='%s'", log);

    make_bin(bin, sizeof(bin), 0);
    fake_tool(bin, "notify-send",
              "#!/bin/sh\nfor a in \"$@\"; do printf '[%s]\\n' \"$a\"; done >> \"$FAKE_LOG\"\n");
    command[0] = '\0';
    require(host_append_notify_command(command, sizeof(command), "--help", "-e hello & $HOME"),
            "notify command should build");
    require(run_command(bin, home, env, command, out) == 0, "notify-send path should succeed");
    require_text(read_file(log), "[--]\n[--help]\n[-e hello & $HOME]\n",
                 "notify-send should get -- before title and message");

    /* osascript: the script comes from stdin, so the data stays operands */
    unlink(log);
    make_bin(bin, sizeof(bin), 0);
    fake_tool(bin, "osascript",
              "#!/bin/sh\nfor a in \"$@\"; do printf '[%s]\\n' \"$a\"; done >> \"$FAKE_LOG\"\n"
              "cat >> \"$FAKE_LOG\"\n");
    command[0] = '\0';
    require(host_append_notify_command(command, sizeof(command), "-e", "-s hi"),
            "notify command should build");
    require(run_command(bin, home, env, command, out) == 0, "osascript path should succeed");
    {
        const char *logged = read_file(log);

        require(strncmp(logged, "[-]\n[-e]\n[-s hi]\n", 17) == 0,
                "osascript should read its script from stdin and get title and message after it");
        require_contains(logged, "display notification (item 2 of argv) with title (item 1 of argv)",
                         "osascript should receive the notification script");
    }

    make_bin(bin, sizeof(bin), 0);
    require(run_command(bin, home, env, command, out) == 127, "no notifier should give 127");
}

/* --- host-edit --------------------------------------------------- */

static void edit_fakes(const char *bin, int with_gtk_launch, int gtk_rc)
{
    char script[256];

    fake_tool(bin, "xdg-mime",
              "#!/bin/sh\necho \"xdg-mime $*\" >> \"$FAKE_LOG\"\n"
              "case \"$*\" in\n"
              "\"query default text/plain\") printf '%s\\n' \"${FAKE_TEXT_DESKTOP-editor.desktop}\";;\n"
              "\"query filetype\"*) echo image/png;;\n"
              "\"query default image/png\") echo viewer.desktop;;\n"
              "esac\n");
    if (with_gtk_launch) {
        snprintf(script, sizeof(script),
                 "#!/bin/sh\necho \"gtk-launch $*\" >> \"$FAKE_LOG\"\nexit %d\n", gtk_rc);
        fake_tool(bin, "gtk-launch", script);
    }
    fake_tool(bin, "xdg-open", "#!/bin/sh\necho \"xdg-open $*\" >> \"$FAKE_LOG\"\n");
    fake_tool(bin, "myeditor", "#!/bin/sh\necho \"myeditor $*\" >> \"$FAKE_LOG\"\n");
    fake_tool(bin, "visualeditor", "#!/bin/sh\necho \"visualeditor $*\" >> \"$FAKE_LOG\"\n");
}

static const char *run_edit(const char *bin, const char *home, const char *path,
                            const char *extra_env, int *rc)
{
    char command[HOST_MAX_COMMAND_LEN];
    char env[500];
    char log[260];
    char out[260];

    snprintf(log, sizeof(log), "%s/edit.log", home);
    unlink(log);
    snprintf(out, sizeof(out), "%s/out", home);
    snprintf(env, sizeof(env), "FAKE_LOG='%s' %s", log, extra_env);
    command[0] = '\0';
    require(host_append_edit_command(command, sizeof(command), path), "edit command should build");
    *rc = run_command(bin, home, env, command, out);
    return read_file(log);
}

static void test_edit_selects_text_editor(void)
{
    char bin[200], home[200];
    const char *log;
    int rc;

    make_dir(home, sizeof(home), "home");

    /* the target's own type (a PNG here) is never consulted */
    make_bin(bin, sizeof(bin), 0);
    edit_fakes(bin, 1, 0);
    log = run_edit(bin, home, "/tmp/pic.png", "", &rc);
    require(rc == 0, "edit should succeed");
    require_text(log, "xdg-mime query default text/plain\n"
                      "gtk-launch editor.desktop file:///tmp/pic.png\n",
                 "edit should launch the text/plain association with the file URI");

    /* gtk-launch fails: VISUAL, then xdg-open is not reached */
    make_bin(bin, sizeof(bin), 0);
    edit_fakes(bin, 1, 1);
    log = run_edit(bin, home, "/tmp/pic.png", "VISUAL=visualeditor EDITOR=myeditor", &rc);
    require(rc == 0, "edit should succeed through VISUAL");
    require_contains(log, "visualeditor /tmp/pic.png\n", "VISUAL should be tried before EDITOR");
    require_not_contains(log, "myeditor", "EDITOR should not run when VISUAL worked");
    require_not_contains(log, "xdg-open", "xdg-open is the last resort");

    /* no association, no gtk-launch: EDITOR */
    make_bin(bin, sizeof(bin), 0);
    edit_fakes(bin, 0, 0);
    log = run_edit(bin, home, "/tmp/doc.pdf", "EDITOR=myeditor FAKE_TEXT_DESKTOP=", &rc);
    require(rc == 0, "edit should succeed through EDITOR");
    require_text(log, "xdg-mime query default text/plain\nmyeditor /tmp/doc.pdf\n",
                 "EDITOR should be used before xdg-open");

    /* a relative path has no URI: skip gtk-launch */
    make_bin(bin, sizeof(bin), 0);
    edit_fakes(bin, 1, 0);
    log = run_edit(bin, home, "new.txt", "EDITOR=myeditor", &rc);
    require(rc == 0, "edit should succeed for a relative path");
    require_text(log, "xdg-mime query default text/plain\nmyeditor new.txt\n",
                 "relative paths cannot use gtk-launch");

    /* nothing else: xdg-open */
    make_bin(bin, sizeof(bin), 0);
    edit_fakes(bin, 0, 0);
    log = run_edit(bin, home, "/tmp/pic.png", "", &rc);
    require(rc == 0, "edit should succeed through xdg-open");
    require_text(log, "xdg-mime query default text/plain\nxdg-open /tmp/pic.png\n",
                 "xdg-open should be the last resort");

    /* nothing at all */
    make_bin(bin, sizeof(bin), 0);
    log = run_edit(bin, home, "/tmp/pic.png", "", &rc);
    require(rc == 127, "edit without any launcher should fail");
    (void)log;
}

static const struct {
    const char *name;
    void (*run)(void);
} tests[] = {
    { "env-get", test_env_get_finds_user_variable_named_like_scratch_state },
    { "env-parallel", test_env_parallel_updates },
    { "env-signal", test_env_signal_cleanup },
    { "env-lock-timeout", test_env_lock_timeout },
    { "download-metadata", test_download_metadata_line },
    { "download-tools", test_download_wget_and_missing_tools },
    { "download-legacy", test_download_legacy_heartbeat },
    { "notify", test_notify_arguments_are_not_options },
    { "edit", test_edit_selects_text_editor },
};

/* With a test name as the only argument, run just that test. */
int main(int argc, char *argv[])
{
    const char *selected = getenv("HOST_TEST_SHELL");
    char tmpl[] = "/tmp/host-tools-posix.XXXXXX";

    if (selected != NULL && selected[0] != '\0') {
        shell = selected;
    }
    require(mkdtemp(tmpl) != NULL, "test root should be created");
    snprintf(root, sizeof(root), "%s", tmpl);

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        if (argc > 1 && strcmp(argv[1], tests[i].name) != 0) {
            continue;
        }
        tests[i].run();
    }

    sh("rm -rf '%s'", root);
    return 0;
}
