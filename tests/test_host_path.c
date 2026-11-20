/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_path_logic.h"

/* A small fake AmigaDOS: objects with their native host paths */
struct object {
    const char *path;
    int is_dir;
    const char *native;
    long lock_error;
};

static const struct object objects[] = {
    { "HT:", 1, "/home/u/ht", 0 },
    { "HT:docs", 1, "/home/u/ht/docs", 0 },
    { "HT:docs/a.txt", 0, "/home/u/ht/docs/a.txt", 0 },
    { "HT:caf\xE9.txt", 0, "/home/u/ht/caf\xC3\xA9.txt", 0 },
    { "DATA:", 1, "/data", 0 },
    { "DATA:readme", 0, "/data/readme", 0 },
    { "FILE:x", 0, "/files/x", 0 },
    { "SYS:", 1, "/sys", 0 },
    { "SYS:ghost", 0, NULL, 218 },
    { "WH:", 1, "C:\\Users\\caf\xC3\xA9", 0 },
    { "WROOT:", 1, "C:\\", 0 },
    { "RAM:", 1, NULL, 0 },
    { "DF0:", 1, NULL, 226 },
    { "", 1, "/cwd", 0 },
    { "readme.txt", 0, "/cwd/readme.txt", 0 },
    { ":top", 0, "/sys/top", 0 },
    { NULL, 0, NULL, 0 }
};

static const char *const dos_names[] = {
    "HT", "DATA", "FILE", "SYS", "WH", "WROOT", "RAM", "DF0", "C", NULL
};

static int lock_calls;
static int open_locks;
static char last_lock_path[512];

static int equal_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (host_ascii_lower(*a) != host_ascii_lower(*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static host_lock_t fake_lock(const char *path, long *ioerr)
{
    char key[512];
    size_t len = strlen(path);

    lock_calls++;
    strncpy(last_lock_path, path, sizeof(last_lock_path) - 1);

    /* a trailing slash after a name is ignored, as in AmigaDOS */
    memcpy(key, path, len + 1);
    if (len > 0 && key[len - 1] == '/') {
        key[len - 1] = '\0';
    }

    for (const struct object *o = objects; o->path != NULL; o++) {
        if (equal_ci(o->path, key)) {
            if (o->lock_error != 0) {
                *ioerr = o->lock_error;
                return 0;
            }
            open_locks++;
            return (host_lock_t)(unsigned long)o;
        }
    }

    *ioerr = HOST_DOS_ERR_OBJECT_NOT_FOUND;
    return 0;
}

static void fake_unlock(host_lock_t lock)
{
    (void)lock;
    open_locks--;
}

static int fake_is_dir(host_lock_t lock)
{
    return ((const struct object *)(unsigned long)lock)->is_dir;
}

static int fake_native(host_lock_t lock, char *out, size_t out_size)
{
    const struct object *o = (const struct object *)(unsigned long)lock;

    if (o->native == NULL) {
        return 1;
    }
    strncpy(out, o->native, out_size - 1);
    return 0;
}

static int fake_dos_name_exists(const char *name)
{
    for (int i = 0; dos_names[i] != NULL; i++) {
        if (equal_ci(dos_names[i], name)) {
            return 1;
        }
    }
    return 0;
}

static const struct host_path_ops ops = {
    fake_lock, fake_unlock, fake_is_dir, fake_native, fake_dos_name_exists
};

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static int resolve(int mode, const char *arg, char *out, size_t out_size)
{
    lock_calls = 0;
    open_locks = 0;
    last_lock_path[0] = '\0';
    out[0] = '\0';
    return host_path_resolve_with(&ops, mode, arg, out, out_size);
}

static void require_status(int mode, const char *arg, int expected, const char *native,
                           int expected_locks, const char *message)
{
    char out[256];
    int status = resolve(mode, arg, out, sizeof(out));

    if (status != expected || (native != NULL && strcmp(out, native) != 0) ||
        (expected_locks >= 0 && lock_calls != expected_locks)) {
        fprintf(stderr, "%s\n  arg: %s\n  status %d (expected %d), locks %d (expected %d), out: %s\n",
                message, arg, status, expected, lock_calls, expected_locks, out);
        exit(1);
    }
    require(open_locks == 0, "every lock taken should be released");
}

static void test_existing_paths(void)
{
    require_status(HOST_PATH_MODE_EXISTING, "HT:docs/a.txt", HOST_PATH_OK,
                   "/home/u/ht/docs/a.txt", 1, "existing path should translate");
    require_status(HOST_PATH_MODE_FILE, "ht:DOCS", HOST_PATH_OK, "/home/u/ht/docs", 1,
                   "volume and path case should not matter");
    require_status(HOST_PATH_MODE_ARG, "readme.txt", HOST_PATH_OK, "/cwd/readme.txt", 1,
                   "existing relative path should translate");
    require_status(HOST_PATH_MODE_FILE, ":top", HOST_PATH_OK, "/sys/top", 1,
                   "path on the current volume should translate");
    require_status(HOST_PATH_MODE_FILE, "HT:caf\xE9.txt", HOST_PATH_OK,
                   "/home/u/ht/caf\xC3\xA9.txt", 1,
                   "translated path should be returned as the UTF-8 the host gave");
}

static void test_missing_amiga_paths_are_errors(void)
{
    /* the reported bug: HT:no-such-file used to reach the host as a string */
    require_status(HOST_PATH_MODE_FILE, "HT:no-such-file", HOST_PATH_NOT_FOUND, NULL, 1,
                   "missing file on a known volume should be an error");
    require_status(HOST_PATH_MODE_ARG, "HT:no-such-file", HOST_PATH_NOT_FOUND, NULL, 1,
                   "missing file on a known volume should be an error for host-run too");
    require_status(HOST_PATH_MODE_EXISTING, "HT:no-such-file", HOST_PATH_NOT_FOUND, NULL, 1,
                   "host-path should report a missing file");
    require_status(HOST_PATH_MODE_FILE, "nosuchfile", HOST_PATH_NOT_FOUND, NULL, 1,
                   "a missing relative path is an error where a path is required");
    require_status(HOST_PATH_MODE_FILE, "NOPE:file", HOST_PATH_NO_VOLUME, NULL, 0,
                   "an unknown volume is an error where a path is required");
    require_status(HOST_PATH_MODE_EXISTING, "NOPE:file", HOST_PATH_NO_VOLUME, NULL, 0,
                   "host-path should report an unknown volume");
    require_status(HOST_PATH_MODE_FILE, "SYS:ghost", HOST_PATH_NO_VOLUME, NULL, 1,
                   "a device that is not mounted should be reported as such");
    require_status(HOST_PATH_MODE_FILE, "DF0:", HOST_PATH_LOCK_ERROR, NULL, 1,
                   "other lock failures should not become host paths");
    require_status(HOST_PATH_MODE_FILE, "RAM:", HOST_PATH_NATIVE_ERROR, NULL, 1,
                   "objects without a host path should fail");
}

static void test_host_arguments_pass_through(void)
{
    require_status(HOST_PATH_MODE_ARG, "hello", HOST_PATH_HOST, NULL, 1,
                   "a plain word that is not a file is a host argument");
    require_status(HOST_PATH_MODE_ARG, "-v", HOST_PATH_HOST, NULL, 1,
                   "an option is a host argument");
    require_status(HOST_PATH_MODE_ARG, "NOPE:file", HOST_PATH_HOST, NULL, 0,
                   "text with an unknown volume is a host argument for host-run");
    require_status(HOST_PATH_MODE_ARG, "12:30", HOST_PATH_HOST, NULL, 0,
                   "a time is a host argument");
    require_status(HOST_PATH_MODE_ARG, "--out:name", HOST_PATH_HOST, NULL, 0,
                   "an option with a colon is a host argument");
    require_status(HOST_PATH_MODE_ARG, "", HOST_PATH_HOST, NULL, 0,
                   "an empty argument is passed on by host-run");
    require_status(HOST_PATH_MODE_FILE, "", HOST_PATH_EMPTY, NULL, 0,
                   "an empty path is an error where a path is required");
}

static void test_uri_schemes_and_dos_names(void)
{
    require_status(HOST_PATH_MODE_FILE, "https://example.com/a?b=1", HOST_PATH_URI, NULL, 0,
                   "a URL is not an Amiga path");
    require_status(HOST_PATH_MODE_FILE, "mailto:a@b.c", HOST_PATH_URI, NULL, 0,
                   "a URI scheme without a volume of that name is a URI");
    require_status(HOST_PATH_MODE_EXISTING, "https://example.com", HOST_PATH_URI, NULL, 0,
                   "host-path should refuse URLs");

    /* volumes named like schemes keep their Amiga meaning */
    require_status(HOST_PATH_MODE_EXISTING, "DATA:readme", HOST_PATH_OK, "/data/readme", 1,
                   "DATA: is a volume, not a data: URI");
    require_status(HOST_PATH_MODE_FILE, "data:readme", HOST_PATH_OK, "/data/readme", 1,
                   "scheme names match volumes case-insensitively");
    require_status(HOST_PATH_MODE_FILE, "FILE:x", HOST_PATH_OK, "/files/x", 1,
                   "FILE: is a volume, not a file: URI");
    require_status(HOST_PATH_MODE_FILE, "DATA:missing", HOST_PATH_NOT_FOUND, NULL, 1,
                   "a missing file on DATA: is an error, not a URI");
    require_status(HOST_PATH_MODE_ARG, "DATA:readme", HOST_PATH_OK, "/data/readme", 1,
                   "host-run should translate DATA:readme");
}

static void test_long_arguments(void)
{
    char arg[600];
    char out[256];
    int status;

    memset(arg, 'a', 300);
    arg[300] = '\0';
    require_status(HOST_PATH_MODE_ARG, arg, HOST_PATH_HOST, NULL, 0,
                   "a long ordinary argument must not be probed as a path");
    require_status(HOST_PATH_MODE_FILE, arg, HOST_PATH_ARG_TOO_LONG, NULL, 0,
                   "a long path must be rejected before Lock()");

    memcpy(arg, "HT:", 3);
    require_status(HOST_PATH_MODE_ARG, arg, HOST_PATH_ARG_TOO_LONG, NULL, 0,
                   "a long path on a known volume must be rejected before Lock()");
    require_status(HOST_PATH_MODE_NEW, arg, HOST_PATH_ARG_TOO_LONG, NULL, 0,
                   "a long new path must be rejected before Lock()");

    memcpy(arg, "https://", 8);
    require_status(HOST_PATH_MODE_FILE, arg, HOST_PATH_URI, NULL, 0,
                   "a long URL is still a URL");

    memcpy(arg, "HT:", 3);
    memset(arg + 3, 'b', 252);
    arg[255] = '\0';
    status = resolve(HOST_PATH_MODE_FILE, arg, out, sizeof(out));
    require(status == HOST_PATH_NOT_FOUND && lock_calls == 1 && strlen(last_lock_path) == 255,
            "a path of exactly 255 characters is looked up");
    arg[255] = 'b';
    arg[256] = '\0';
    status = resolve(HOST_PATH_MODE_FILE, arg, out, sizeof(out));
    require(status == HOST_PATH_ARG_TOO_LONG && lock_calls == 0,
            "a path of 256 characters is not looked up");
}

static void test_windows_paths(void)
{
    /* C: is an Amiga assign here, but C:\... is a Windows path */
    require_status(HOST_PATH_MODE_ARG, "C:\\Users\\me\\a.txt", HOST_PATH_HOST, NULL, 0,
                   "a Windows path is a host argument for host-run");
    require_status(HOST_PATH_MODE_FILE, "C:\\Users\\me\\a.txt", HOST_PATH_HOST, NULL, 0,
                   "a Windows path is passed on by host-multiview and host-reveal");
    require_status(HOST_PATH_MODE_NEW, "d:\\x\\new.txt", HOST_PATH_HOST, NULL, 0,
                   "a Windows path is passed on by host-edit");
    require_status(HOST_PATH_MODE_FILE, "\\\\server\\share\\a.txt", HOST_PATH_HOST, NULL, 0,
                   "a UNC path is passed on");
    require_status(HOST_PATH_MODE_EXISTING, "C:\\Users\\me\\a.txt", HOST_PATH_NOT_FOUND, NULL, 1,
                   "host-path only knows Amiga paths");
    require_status(HOST_PATH_MODE_FILE, "C:/Users", HOST_PATH_NOT_FOUND, NULL, 1,
                   "a slash after the volume is an Amiga path");
}

static void test_new_files(void)
{
    require_status(HOST_PATH_MODE_NEW, "HT:newfile.txt", HOST_PATH_OK,
                   "/home/u/ht/newfile.txt", 2,
                   "a new file in an existing directory should translate its parent");
    require_status(HOST_PATH_MODE_NEW, "HT:docs/new.txt", HOST_PATH_OK,
                   "/home/u/ht/docs/new.txt", 2, "a new file in a subdirectory");
    require_status(HOST_PATH_MODE_NEW, "newfile.txt", HOST_PATH_OK, "/cwd/newfile.txt", 2,
                   "a new relative file goes into the current directory");
    require_status(HOST_PATH_MODE_NEW, "HT:docs/caf\xE9.txt", HOST_PATH_OK,
                   "/home/u/ht/docs/caf\xC3\xA9.txt", 2,
                   "the new name is converted from ISO-8859-1 to UTF-8");
    require_status(HOST_PATH_MODE_NEW, "WH:new.txt", HOST_PATH_OK,
                   "C:\\Users\\caf\xC3\xA9\\new.txt", 2,
                   "a Windows parent gets a backslash");
    require_status(HOST_PATH_MODE_NEW, "WROOT:new.txt", HOST_PATH_OK, "C:\\new.txt", 2,
                   "a parent that ends in a separator gets no second one");
    require_status(HOST_PATH_MODE_NEW, "DATA:readme", HOST_PATH_OK, "/data/readme", 1,
                   "an existing file is not treated as new");

    require_status(HOST_PATH_MODE_NEW, "HT:nodir/new.txt", HOST_PATH_NOT_FOUND, NULL, 2,
                   "a new file needs an existing directory");
    require_status(HOST_PATH_MODE_NEW, "HT:docs/a.txt/b", HOST_PATH_NOT_DIR, NULL, 2,
                   "the parent of a new file must be a directory");
    require_status(HOST_PATH_MODE_NEW, "HT:", HOST_PATH_OK, "/home/u/ht", 1,
                   "an existing directory is accepted as is");
    require_status(HOST_PATH_MODE_NEW, "HT:docs/nofile/", HOST_PATH_NOT_FOUND, NULL, 1,
                   "a trailing slash names no new file");
    require_status(HOST_PATH_MODE_NEW, "RAM:new.txt", HOST_PATH_NATIVE_ERROR, NULL, 2,
                   "a new file in a directory without a host path should fail");
    require_status(HOST_PATH_MODE_NEW, "NOPE:new.txt", HOST_PATH_NO_VOLUME, NULL, 0,
                   "a new file on an unknown volume should fail");
    require_status(HOST_PATH_MODE_NEW, "DF0:new.txt", HOST_PATH_LOCK_ERROR, NULL, 2,
                   "a new file on an unreadable volume should fail");
    require_status(HOST_PATH_MODE_NEW, "HT:a*b", HOST_PATH_BAD_NAME, NULL, 1,
                   "a new name with characters no host takes should fail");
    require_status(HOST_PATH_MODE_NEW, "HT:a\"b", HOST_PATH_BAD_NAME, NULL, 1,
                   "a quote in a new name should fail");
    require_status(HOST_PATH_MODE_FILE, "HT:newfile.txt", HOST_PATH_NOT_FOUND, NULL, 1,
                   "only host-edit creates files");
}

static void test_buffer_limits(void)
{
    char small[8];
    char out[16];

    require(resolve(HOST_PATH_MODE_FILE, "HT:docs/a.txt", small, sizeof(small)) ==
                HOST_PATH_TOO_LONG,
            "a native path that does not fit should be reported");
    require(resolve(HOST_PATH_MODE_NEW, "HT:docs/new.txt", out, sizeof(out)) ==
                HOST_PATH_TOO_LONG,
            "a new file whose path does not fit should be reported");
}

static void test_error_texts(void)
{
    for (int status = HOST_PATH_EMPTY; status <= HOST_PATH_BAD_NAME; status++) {
        if (status == HOST_PATH_HOST) {
            continue;
        }
        require(strcmp(host_path_error(status), "unknown path error") != 0,
                "every error status should have a message");
    }
}

int main(void)
{
    test_existing_paths();
    test_missing_amiga_paths_are_errors();
    test_host_arguments_pass_through();
    test_uri_schemes_and_dos_names();
    test_long_arguments();
    test_windows_paths();
    test_new_files();
    test_buffer_limits();
    test_error_texts();
    return 0;
}
