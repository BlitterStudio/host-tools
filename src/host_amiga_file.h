/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_AMIGA_FILE_H
#define HOST_AMIGA_FILE_H

#include <stdio.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <proto/exec.h>

#include "host_download_command.h"

/*
 * Guest-side file helpers shared by the tools that write a file next to
 * its final name and then move it into place.
 */

/* Lock without "please insert volume" requesters */
static BPTR host_quiet_lock(const char *path)
{
    struct Process *proc = (struct Process *)FindTask(NULL);
    APTR old_window = proc->pr_WindowPtr;
    BPTR lock;

    proc->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)path, ACCESS_READ);
    proc->pr_WindowPtr = old_window;
    return lock;
}

static int host_lock_is_directory(BPTR lock)
{
    static struct FileInfoBlock fib __attribute__((aligned(4)));

    if (!Examine(lock, &fib)) {
        return 0;
    }
    return fib.fib_DirEntryType > 0;
}

static int host_make_unused_sidecar_path(const char *destination, char kind,
                                         char *path, size_t path_size)
{
    unsigned long token = (unsigned long)FindTask(NULL);

    for (unsigned int attempt = 0; attempt < 100; attempt++) {
        BPTR lock;

        if (!host_download_sidecar_path(destination, kind, token, attempt,
                                        path, path_size)) {
            return 0;
        }
        lock = host_quiet_lock(path);
        if (lock == 0) {
            return 1;
        }
        UnLock(lock);
    }
    return 0;
}

/*
 * Move temp_path onto destpath. An existing file is renamed to a backup
 * first and restored when the move fails; it is replaced only with
 * force. move_failure and appeared_failure are the messages for the
 * two failures whose wording depends on the caller.
 */
static int host_install_file(const char *temp_path, const char *destpath, int force,
                             char *backup_path, size_t backup_size,
                             const char *move_failure, const char *appeared_failure,
                             const char **failure)
{
    BPTR lock = host_quiet_lock(destpath);

    if (lock == 0) {
        if (Rename((STRPTR)temp_path, (STRPTR)destpath)) {
            return 1;
        }
        *failure = move_failure;
        return 0;
    }

    if (host_lock_is_directory(lock)) {
        UnLock(lock);
        *failure = "Destination path is a directory";
        return 0;
    }
    UnLock(lock);

    if (!force) {
        *failure = appeared_failure;
        return 0;
    }
    if (!host_make_unused_sidecar_path(destpath, 'b', backup_path, backup_size)) {
        *failure = "Cannot reserve a destination backup";
        return 0;
    }
    if (!Rename((STRPTR)destpath, (STRPTR)backup_path)) {
        *failure = "Cannot preserve the existing destination";
        return 0;
    }
    if (Rename((STRPTR)temp_path, (STRPTR)destpath)) {
        if (!DeleteFile((STRPTR)backup_path)) {
            printf("\nWarning: old destination remains at %s\n", backup_path);
        }
        return 1;
    }

    if (!Rename((STRPTR)backup_path, (STRPTR)destpath)) {
        printf("\nOriginal destination remains at %s\n", backup_path);
    }
    *failure = "Cannot replace destination";
    return 0;
}

#endif
