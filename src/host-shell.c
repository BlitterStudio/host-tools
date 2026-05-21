/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "host_capture.h"
#include "host_common.h"

#define OUTBUFSIZE 4095

static const char version[] = "$VER: Host-Shell " VERSION_STR " (" DATE_STR ")";
static char outbuf[OUTBUFSIZE + 1];

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char buffer[4096];
    BPTR in = 0;
    BPTR out = 0;
    long handle = 0;
    BOOL esc_pending = FALSE;
    BOOL raw_mode = FALSE;
    long actual;
    ULONG status;
    int status_supported = 0;
    int return_code = 0;

    command[0] = '\0';

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc == 2 && strcmp(argv[1], "?") == 0)
    {
        printf("Host-Shell v%s\n", VERSION_STR);
        printf("Host-Shell opens an interactive terminal session on the host system.\n");
        printf("%s\nUsage: host-shell [command]\n", version);
        return 0;
    }

    if (argc == 2) {
        size_t command_len = strlen(argv[1]);
        if (command_len >= sizeof(command)) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }
        memcpy(command, argv[1], command_len + 1);
    } else {
        // Combine arguments into a safely quoted command string.
        for (int i = 1; i < argc; i++)
        {
            if (!host_append_shell_arg(command, sizeof(command), argv[i], i > 1)) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
        }
    }

    if ((DOSBase = (struct DosLibrary *)OpenLibrary((UBYTE *)"dos.library", 0)) == NULL) {
        printf("dos.library not found!\n");
        return HOST_RETURN_ERROR;
    }

    in = Input();
    out = Output();
    if (in == 0 || out == 0) {
        printf("Failed to access console IO.\n");
        return_code = HOST_RETURN_ERROR;
        goto cleanup;
    }

    // Enable Raw Mode
    if (!SetMode(in, 1)) {
        printf("Failed to enable raw input mode.\n");
        return_code = HOST_RETURN_ERROR;
        goto cleanup;
    }
    raw_mode = TRUE;

    handle = HostShell_Open((UBYTE *)command);
    if (handle == 0) {
        printf("Failed to open host shell session.\n");
        return_code = HOST_RETURN_ERROR;
        goto cleanup;
    }

    for (;;)
    {
        // Check for output from host
        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer) - 2);
        if (actual > 0)
        {
            int outptr = 0;
            for (int i = 0; i < actual; i++) {
                unsigned char c = (unsigned char)buffer[i];
                if (esc_pending) {
                    if (c == 0x5B) { // '['
                        outbuf[outptr++] = 0x9B; // CSI
                    } else {
                        outbuf[outptr++] = 0x1B; // Original ESC
                        outbuf[outptr++] = c;
                    }
                    esc_pending = FALSE;
                } else {
                    if (c == 0x1B) {
                        esc_pending = TRUE;
                    } else {
                        outbuf[outptr++] = c;
                    }
                }

                // Safety check for outbuf overflow (should rarely happen given the math)
                if (outptr >= OUTBUFSIZE) {
                    Write(out, outbuf, outptr);
                    outptr = 0;
                }
            }
            if (outptr > 0) {
                Write(out, outbuf, outptr);
            }
        }
        else if (actual < 0) // Error or closed
        {
            status = HostShell_Status(handle);
            if (status == HOST_SHELL_STATUS_INVALID) {
                return_code = status_supported ? HOST_RETURN_ERROR : 0;
            } else {
                return_code = host_capture_status_rc(status);
            }
            break;
        }
        else {
            status = HostShell_Status(handle);
            if (status != HOST_SHELL_STATUS_INVALID) {
                status_supported = 1;
            }
            if ((status & HOST_SHELL_STATUS_EXITED) != 0) {
                return_code = host_capture_status_rc(status);
                break;
            }
            if (status == HOST_SHELL_STATUS_INVALID && status_supported) {
                return_code = HOST_RETURN_ERROR;
                break;
            }
        }

        // Check for input from Amiga user
        // Wait up to 20ms (20000 microseconds)
        if (WaitForChar(in, 20000))
        {
            // Read less than buffer size to allow for expansion (max 2x)
            actual = Read(in, buffer, 1024);
            if (actual > 0)
            {
                int outptr = 0;
                for (int i = 0; i < actual; i++) {
                    unsigned char c = (unsigned char)buffer[i];
                    if (c == 0x9B) { // Amiga CSI
                        // Convert to ANSI ESC [
                        outbuf[outptr++] = 0x1B;
                        outbuf[outptr++] = 0x5B;
                    } else if (c == 0x08) { // Backspace
                        // Convert BS (0x08) to DEL (0x7F)
                        outbuf[outptr++] = 0x7F;
                    } else {
                        outbuf[outptr++] = c;
                    }
                }
                if (outptr > 0) {
                    HostShell_Write(handle, (UBYTE *)outbuf, outptr);
                }
            }
            else if (actual == 0) // EOF
            {
                // Maybe break? Or just ignore?
            }
        }

        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
        {
            SetSignal(0, SIGBREAKF_CTRL_C); // Clear the signal
            // Send Ctrl-C (ETX) to host
            char ctrlc = 0x03;
            HostShell_Write(handle, (UBYTE *)&ctrlc, 1);
        }
    }

cleanup:
    if (esc_pending && out != 0) {
        outbuf[0] = 0x1B;
        Write(out, outbuf, 1);
    }
    if (handle != 0) {
        HostShell_Close(handle);
    }
    if (raw_mode) {
        SetMode(in, 0); // Restore Cooked Mode
    }
    CloseLibrary((struct Library *)DOSBase);
    return return_code;
}
