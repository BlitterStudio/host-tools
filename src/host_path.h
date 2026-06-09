/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_PATH_H
#define HOST_PATH_H

#include <dos/dosextens.h>

#include "host_common.h"
#include "uae_pragmas.h"

#define HOST_PATH_OK 0
#define HOST_PATH_EMPTY 1
#define HOST_PATH_URI 2
#define HOST_PATH_NOT_FOUND 3
#define HOST_PATH_NATIVE_ERROR 4
#define HOST_PATH_TOO_LONG 5

static inline int host_resolve_existing_path(const char *arg, char *out, size_t out_size)
{
    struct Process *proc;
    APTR old_window;
    BPTR lock;

    if (arg == NULL || out == NULL || out_size == 0) {
        return HOST_PATH_NATIVE_ERROR;
    }

    if (arg[0] == '\0') {
        return HOST_PATH_EMPTY;
    }

    if (host_is_uri(arg)) {
        return HOST_PATH_URI;
    }

    /* suppress "please insert volume" requesters while probing arguments */
    proc = (struct Process *)FindTask(NULL);
    old_window = proc->pr_WindowPtr;
    proc->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)arg, ACCESS_READ);
    proc->pr_WindowPtr = old_window;
    if (!lock) {
        return HOST_PATH_NOT_FOUND;
    }

    out[0] = '\0';
    out[out_size - 1] = '\0';
    if (NativeDosOp(0, (ULONG)lock, (ULONG)out, out_size) != 0) {
        UnLock(lock);
        return HOST_PATH_NATIVE_ERROR;
    }

    UnLock(lock);
    if (host_filled_buffer(out, out_size)) {
        return HOST_PATH_TOO_LONG;
    }

    return HOST_PATH_OK;
}

static inline int host_resolve_optional_path(const char *arg, char *out, size_t out_size,
                                             const char **target)
{
    int status;

    *target = arg;
    if (arg[0] == '\0' || host_is_uri(arg)) {
        return HOST_PATH_OK;
    }

    status = host_resolve_existing_path(arg, out, out_size);
    if (status == HOST_PATH_OK) {
        *target = out;
        return HOST_PATH_OK;
    }

    if (status == HOST_PATH_NOT_FOUND) {
        return HOST_PATH_OK;
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
        default:
            return "unknown path error";
    }
}

#endif
