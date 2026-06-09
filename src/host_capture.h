/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CAPTURE_H
#define HOST_CAPTURE_H

#include <stdio.h>
#include "host_common.h"
#include "uae_pragmas.h"

#define HOST_CAPTURE_IDLE_LIMIT 250
#ifndef HOST_SHELL_STATUS_INVALID
#define HOST_SHELL_STATUS_INVALID 0UL
#endif
#ifndef HOST_SHELL_STATUS_RUNNING
#define HOST_SHELL_STATUS_RUNNING 1UL
#endif
#ifndef HOST_SHELL_STATUS_EXITED
#define HOST_SHELL_STATUS_EXITED 0x80000000UL
#endif

static inline int host_capture_status_rc(ULONG status)
{
    ULONG exit_code;

    if ((status & HOST_SHELL_STATUS_EXITED) == 0) {
        return HOST_RETURN_ERROR;
    }

    exit_code = status & 0xff;
    return exit_code == 0 ? 0 : HOST_RETURN_ERROR;
}

static inline int host_print_command_output(const char *command)
{
    long handle;
    char buffer[1024];
    long actual;
    ULONG status;
    int idle_count = 0;
    int status_supported = 0;

    /* prefer a pipe session: binary-safe output without terminal
     * line-ending processing; older builds fall back to the pty */
    handle = HostShell_OpenPipe((UBYTE *)command);
    if (handle == 0) {
        handle = HostShell_Open((UBYTE *)command);
    }
    if (handle == 0) {
        printf("Failed to open host command.\n");
        return HOST_RETURN_ERROR;
    }

    while (idle_count < HOST_CAPTURE_IDLE_LIMIT) {
        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer));
        if (actual > 0) {
            fwrite(buffer, 1, actual, stdout);
            idle_count = 0;
        } else if (actual < 0) {
            status = HostShell_Status(handle);
            HostShell_Close(handle);
            fflush(stdout);
            if (status == HOST_SHELL_STATUS_INVALID) {
                return status_supported ? HOST_RETURN_ERROR : 0;
            }
            return host_capture_status_rc(status);
        } else {
            status = HostShell_Status(handle);
            if (status != HOST_SHELL_STATUS_INVALID) {
                status_supported = 1;
            }
            if ((status & HOST_SHELL_STATUS_EXITED) != 0) {
                HostShell_Close(handle);
                fflush(stdout);
                return host_capture_status_rc(status);
            }
            if (status == HOST_SHELL_STATUS_INVALID && status_supported) {
                HostShell_Close(handle);
                fflush(stdout);
                return HOST_RETURN_ERROR;
            }
            if (status_supported) {
                Delay(1);
                continue;
            }
            Delay(1);
            idle_count++;
        }
    }

    HostShell_Close(handle);
    printf("\nTimed out waiting for host command output.\n");
    return HOST_RETURN_ERROR;
}

#endif
