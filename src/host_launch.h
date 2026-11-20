/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_LAUNCH_H
#define HOST_LAUNCH_H

#include <stdio.h>
#include "host_capture.h"
#include "host_common.h"
#include "uae_pragmas.h"

/*
 * Start a program on a POSIX host and report whether it could be
 * started. ExecuteOnHost (trap 88) only reports that the command was
 * handed to a background shell, so a missing program looks like
 * success. The script runs in a pipe session instead: it checks for the
 * program, starts it detached and exits at once, and its output and
 * exit status come back here. fallback is the plain command for
 * Amiberry builds without pipe or terminal sessions; it keeps the old
 * behavior, including the missing error report.
 */
static inline int host_launch_on_host(const char *script, const char *fallback)
{
    char buffer[256];
    long handle;
    int idle_count = 0;

    handle = HostShell_OpenPipe((UBYTE *)script);
    if (handle == 0) {
        handle = HostShell_Open((UBYTE *)script);
    }
    if (handle == 0) {
        return ExecuteOnHost((UBYTE *)fallback) ? 0 : HOST_RETURN_ERROR;
    }

    for (;;) {
        long actual;
        ULONG status;

        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            HostShell_Close(handle);
            printf("\nAborted host command.\n");
            return HOST_RETURN_ERROR;
        }

        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer));
        if (actual > 0) {
            fwrite(buffer, 1, actual, stdout);
            idle_count = 0;
            continue;
        }

        status = HostShell_Status(handle);
        if (actual < 0 || (status & HOST_SHELL_STATUS_EXITED) != 0) {
            HostShell_Close(handle);
            fflush(stdout);
            /* sessions without status reporting (old terminal builds) cannot fail */
            return status == HOST_SHELL_STATUS_INVALID ? 0 : host_capture_status_rc(status);
        }

        Delay(1);
        if (++idle_count >= HOST_CAPTURE_STATUS_IDLE_LIMIT) {
            HostShell_Close(handle);
            printf("\nTimed out waiting for the host command.\n");
            return HOST_RETURN_ERROR;
        }
    }
}

#endif
