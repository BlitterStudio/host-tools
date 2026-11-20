/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_PATH_LOGIC_H
#define HOST_PATH_LOGIC_H

/*
 * Decides what a command line argument is: an Amiga path that exists
 * (translated to a host path), an Amiga path that does not exist (an
 * error), or something that is not an Amiga path at all (a URL or a
 * host argument, used as written). The DOS calls come in through
 * struct host_path_ops so that the decisions can be tested on the PC.
 * host_path.h supplies the real ones.
 */

#include <stddef.h>
#include <string.h>
#include "host_common.h"

#define HOST_PATH_OK 0
#define HOST_PATH_EMPTY 1
/* the argument is a URI: not an Amiga path, used as written */
#define HOST_PATH_URI 2
#define HOST_PATH_NOT_FOUND 3
#define HOST_PATH_NATIVE_ERROR 4
#define HOST_PATH_TOO_LONG 5
#define HOST_PATH_ARG_TOO_LONG 6
#define HOST_PATH_NO_VOLUME 7
#define HOST_PATH_LOCK_ERROR 8
/* not an Amiga path and not a URI (a host argument or Windows path): used as written */
#define HOST_PATH_HOST 9
#define HOST_PATH_NOT_DIR 10
#define HOST_PATH_BAD_NAME 11

/* host-path: the path must exist, nothing is passed through */
#define HOST_PATH_MODE_EXISTING 0
/* host-multiview, host-reveal: an existing path, a URI or a Windows path */
#define HOST_PATH_MODE_FILE 1
/* host-edit: like FILE, and a new file in an existing directory is accepted */
#define HOST_PATH_MODE_NEW 2
/* host-run: any argument; only paths with a known volume must exist */
#define HOST_PATH_MODE_ARG 3

/* AmigaDOS error codes that Lock() reports */
#define HOST_DOS_ERR_DIR_NOT_FOUND 204
#define HOST_DOS_ERR_OBJECT_NOT_FOUND 205
#define HOST_DOS_ERR_DEVICE_NOT_MOUNTED 218

/* the longest volume or assign name looked up in the DOS list */
#define HOST_PATH_NAME_MAX 107

typedef unsigned long host_lock_t;

struct host_path_ops {
    /* shared lock on path, or 0 with the AmigaDOS error code in *ioerr */
    host_lock_t (*lock)(const char *path, long *ioerr);
    void (*unlock)(host_lock_t lock);
    /* 1 for a directory, 0 for anything else, -1 when it cannot be told */
    int (*is_dir)(host_lock_t lock);
    /* native host path of the locked object (UTF-8); 0 on success */
    int (*native)(host_lock_t lock, char *out, size_t out_size);
    /* is name (without the colon) a device, volume or assign in the DOS list? */
    int (*dos_name_exists)(const char *name);
};

/* "C:\dir" or "\\server\share": a Windows path, never an Amiga one */
static inline int host_path_looks_like_windows_path(const char *arg)
{
    char letter = host_ascii_lower(arg[0]);

    return (letter >= 'a' && letter <= 'z' && arg[1] == ':' && arg[2] == '\\') ||
           (arg[0] == '\\' && arg[1] == '\\');
}

static inline int host_path_lock_status(long ioerr)
{
    if (ioerr == HOST_DOS_ERR_OBJECT_NOT_FOUND || ioerr == HOST_DOS_ERR_DIR_NOT_FOUND) {
        return HOST_PATH_NOT_FOUND;
    }
    if (ioerr == HOST_DOS_ERR_DEVICE_NOT_MOUNTED) {
        return HOST_PATH_NO_VOLUME;
    }
    return HOST_PATH_LOCK_ERROR;
}

static inline int host_path_translate(const struct host_path_ops *ops, host_lock_t lock,
                                      char *out, size_t out_size)
{
    out[0] = '\0';
    out[out_size - 1] = '\0';
    if (ops->native(lock, out, out_size) != 0) {
        return HOST_PATH_NATIVE_ERROR;
    }
    if (host_filled_buffer(out, out_size)) {
        return HOST_PATH_TOO_LONG;
    }
    return HOST_PATH_OK;
}

/* characters that no host file system takes in a new file name */
static inline int host_path_leaf_valid(const char *leaf)
{
    for (const unsigned char *p = (const unsigned char *)leaf; *p; p++) {
        if (*p < 0x20 || *p == '"' || *p == '*' || *p == '?' || *p == '<' ||
            *p == '>' || *p == '|' || *p == '\\') {
            return 0;
        }
    }
    return 1;
}

/*
 * A path that does not exist, in a directory that does: translate the
 * directory and append the new name. Both the part before the last
 * separator and the name must be plain Amiga path text.
 */
static inline int host_path_resolve_new(const struct host_path_ops *ops, const char *arg,
                                        char *out, size_t out_size)
{
    char parent[HOST_AMIGA_PATH_MAX + 1];
    size_t split = strlen(arg);
    const char *leaf;
    host_lock_t lock;
    long ioerr = 0;
    char sep = '/';
    size_t len;
    int status;
    int dir;

    while (split > 0 && arg[split - 1] != '/' && arg[split - 1] != ':') {
        split--;
    }
    leaf = arg + split;
    if (leaf[0] == '\0') {
        return HOST_PATH_NOT_FOUND;
    }
    if (!host_path_leaf_valid(leaf)) {
        return HOST_PATH_BAD_NAME;
    }

    memcpy(parent, arg, split);
    parent[split] = '\0';

    lock = ops->lock(parent, &ioerr);
    if (!lock) {
        return host_path_lock_status(ioerr);
    }
    dir = ops->is_dir(lock);
    status = (dir == 1) ? host_path_translate(ops, lock, out, out_size) : HOST_PATH_NOT_DIR;
    ops->unlock(lock);
    if (status != HOST_PATH_OK) {
        return status;
    }

    if (strchr(out, '\\') != NULL) {
        sep = '\\';
    }
    len = strlen(out);
    if (len > 0 && out[len - 1] != '/' && out[len - 1] != '\\' &&
        !host_append_literal(out, out_size, sep == '\\' ? "\\" : "/")) {
        return HOST_PATH_TOO_LONG;
    }
    if (!host_append_latin1_as_utf8(out, out_size, leaf)) {
        return HOST_PATH_TOO_LONG;
    }
    return HOST_PATH_OK;
}

/*
 * Resolve arg. HOST_PATH_OK: out holds the native path (UTF-8).
 * HOST_PATH_URI and HOST_PATH_HOST: not an Amiga path, use arg as
 * written. Anything else is an error for host_path_error().
 *
 * The part before the first colon decides whether arg is an Amiga path:
 * a device, volume or assign in the DOS list makes it one, even when
 * the name looks like a URI scheme (DATA:, FILE:). A Lock() failure on
 * such a path is an error and the string never reaches the host.
 * Arguments without a colon are relative Amiga paths; host-run passes
 * them on as host arguments when they do not exist.
 */
static inline int host_path_resolve_with(const struct host_path_ops *ops, int mode,
                                         const char *arg, char *out, size_t out_size)
{
    const char *colon;
    host_lock_t lock;
    long ioerr = 0;
    int status;
    int has_volume;

    if (arg == NULL || out == NULL || out_size == 0) {
        return HOST_PATH_NATIVE_ERROR;
    }
    if (arg[0] == '\0') {
        return mode == HOST_PATH_MODE_ARG ? HOST_PATH_HOST : HOST_PATH_EMPTY;
    }
    if (mode != HOST_PATH_MODE_EXISTING && host_path_looks_like_windows_path(arg)) {
        return HOST_PATH_HOST;
    }

    colon = strchr(arg, ':');
    has_volume = (colon != NULL);
    if (has_volume) {
        char name[HOST_PATH_NAME_MAX + 1];
        size_t name_len = (size_t)(colon - arg);
        int known = (name_len == 0);

        if (name_len > 0 && name_len <= HOST_PATH_NAME_MAX) {
            memcpy(name, arg, name_len);
            name[name_len] = '\0';
            known = ops->dos_name_exists(name);
        }
        if (!known) {
            if (host_is_uri(arg)) {
                return HOST_PATH_URI;
            }
            return mode == HOST_PATH_MODE_ARG ? HOST_PATH_HOST : HOST_PATH_NO_VOLUME;
        }
    }

    /* AmigaDOS is undefined above 255 characters: do not even try */
    if (strlen(arg) > HOST_AMIGA_PATH_MAX) {
        return (mode == HOST_PATH_MODE_ARG && !has_volume) ? HOST_PATH_HOST
                                                           : HOST_PATH_ARG_TOO_LONG;
    }

    lock = ops->lock(arg, &ioerr);
    if (lock) {
        status = host_path_translate(ops, lock, out, out_size);
        ops->unlock(lock);
        return status;
    }

    status = host_path_lock_status(ioerr);
    if (mode == HOST_PATH_MODE_ARG && !has_volume) {
        return HOST_PATH_HOST;
    }
    if (mode == HOST_PATH_MODE_NEW && status == HOST_PATH_NOT_FOUND) {
        return host_path_resolve_new(ops, arg, out, out_size);
    }
    return status;
}

static inline const char *host_path_error(int status)
{
    switch (status) {
        case HOST_PATH_EMPTY:
            return "empty path";
        case HOST_PATH_URI:
            return "cannot translate URI";
        case HOST_PATH_NOT_FOUND:
            return "path not found";
        case HOST_PATH_NATIVE_ERROR:
            return "native path translation failed";
        case HOST_PATH_TOO_LONG:
            return "resolved host path is too long";
        case HOST_PATH_ARG_TOO_LONG:
            return "path is longer than 255 characters";
        case HOST_PATH_NO_VOLUME:
            return "volume or assign not found";
        case HOST_PATH_LOCK_ERROR:
            return "cannot access path";
        case HOST_PATH_NOT_DIR:
            return "parent is not a directory";
        case HOST_PATH_BAD_NAME:
            return "file name has characters the host cannot use";
        default:
            return "unknown path error";
    }
}

#endif
