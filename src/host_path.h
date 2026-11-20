/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_PATH_H
#define HOST_PATH_H

#include <dos/dosextens.h>

#include "host_common.h"
#include "host_path_logic.h"
#include "uae_pragmas.h"

static host_lock_t UAE_UNUSED host_amiga_lock(const char *path, long *ioerr)
{
    struct Process *proc;
    APTR old_window;
    BPTR lock;

    /* suppress "please insert volume" requesters while probing arguments */
    proc = (struct Process *)FindTask(NULL);
    old_window = proc->pr_WindowPtr;
    proc->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)path, ACCESS_READ);
    *ioerr = lock ? 0 : IoErr();
    proc->pr_WindowPtr = old_window;
    return (host_lock_t)lock;
}

static void UAE_UNUSED host_amiga_unlock(host_lock_t lock)
{
    UnLock((BPTR)lock);
}

static int UAE_UNUSED host_amiga_is_dir(host_lock_t lock)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    int result = -1;

    if (fib != NULL) {
        if (Examine((BPTR)lock, fib)) {
            result = (fib->fib_DirEntryType > 0) ? 1 : 0;
        }
        FreeDosObject(DOS_FIB, fib);
    }
    return result;
}

static int UAE_UNUSED host_amiga_native(host_lock_t lock, char *out, size_t out_size)
{
    return NativeDosOp(0, (ULONG)lock, (ULONG)out, out_size);
}

static int UAE_UNUSED host_amiga_dos_name_exists(const char *name)
{
    struct DosList *list = LockDosList(LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
    int found = (FindDosEntry(list, (CONST_STRPTR)name,
                              LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS) != NULL);

    UnLockDosList(LDF_DEVICES | LDF_VOLUMES | LDF_ASSIGNS | LDF_READ);
    return found;
}

static const struct host_path_ops host_amiga_path_ops = {
    host_amiga_lock,
    host_amiga_unlock,
    host_amiga_is_dir,
    host_amiga_native,
    host_amiga_dos_name_exists
};

/*
 * Resolve arg (see host_path_resolve_with). On HOST_PATH_OK *target is
 * out, the native path (UTF-8). On HOST_PATH_URI and HOST_PATH_HOST it
 * is arg, which is ISO-8859-1 guest text.
 */
static inline int host_resolve_path(int mode, const char *arg, char *out, size_t out_size,
                                    const char **target)
{
    int status = host_path_resolve_with(&host_amiga_path_ops, mode, arg, out, out_size);

    *target = (status == HOST_PATH_OK) ? out : arg;
    return status;
}

/* host-path: the path must exist as an Amiga path */
static inline int host_resolve_existing_path(const char *arg, char *out, size_t out_size)
{
    const char *target;

    return host_resolve_path(HOST_PATH_MODE_EXISTING, arg, out, out_size, &target);
}

/* the argument is usable on the host: translated, a URI or a host argument */
static inline int host_path_usable(int status)
{
    return status == HOST_PATH_OK || status == HOST_PATH_URI || status == HOST_PATH_HOST;
}

/*
 * Windows scripts are UTF-8. A translated path already is; an argument
 * passed on as written is ISO-8859-1 and is converted into out.
 */
static inline int host_path_utf8_target(const char *arg, char *out, size_t out_size,
                                        const char **target)
{
    if (*target != arg) {
        return 1;
    }
    out[0] = '\0';
    if (!host_append_latin1_as_utf8(out, out_size, arg)) {
        return 0;
    }
    *target = out;
    return 1;
}

#endif
