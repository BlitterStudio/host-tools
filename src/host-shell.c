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
#include "host_shell_command.h"
#include "host_terminal_filter.h"

#define OUTBUFSIZE 4095

static const char version[] = "$VER: Host-Shell " VERSION_STR " (" DATE_STR ")";
static char outbuf[OUTBUFSIZE + 1];

/*
 * Scan buf for the console window bounds report "CSI 1;1;<rows>;<cols> r".
 * On a match, store the size and the report's [start, end) byte range.
 */
static int parse_window_report(const char *buf, int len, int *rows, int *cols,
                               int *report_start, int *report_end)
{
    for (int i = 0; i < len; i++) {
        long vals[4] = { 0, 0, 0, 0 };
        int field = 0;
        int j = i + 1;

        if ((unsigned char)buf[i] != 0x9B) {
            continue;
        }

        while (j < len && field < 4) {
            char c = buf[j];
            if (c >= '0' && c <= '9') {
                vals[field] = vals[field] * 10 + (c - '0');
                j++;
            } else if (c == ';' && field < 3) {
                field++;
                j++;
            } else {
                break;
            }
        }

        if (field == 3 && j + 1 < len && buf[j] == ' ' && buf[j + 1] == 'r' &&
            vals[2] > 0 && vals[2] < 1000 && vals[3] > 0 && vals[3] < 1000) {
            *rows = (int)vals[2];
            *cols = (int)vals[3];
            *report_start = i;
            *report_end = j + 2;
            return 1;
        }
    }

    return 0;
}

/*
 * Ask the console for its window size so the host pty can be set up to
 * match. Bytes that are not part of the report are typed-ahead input and
 * are returned in pending for forwarding to the host session.
 */
static int query_console_size(BPTR in, BPTR out, int *rows, int *cols,
                              char *pending, int pending_size, int *pending_len)
{
    static char rbuf[128];
    int rlen = 0;
    int found = 0;
    int report_start = 0;
    int report_end = 0;

    *pending_len = 0;
    Write(out, (APTR)"\x9B" "0 q", 4);

    for (int tries = 0; tries < 10 && rlen < (int)sizeof(rbuf); tries++) {
        long got;

        if (!WaitForChar(in, 50000)) {
            break;
        }
        got = Read(in, rbuf + rlen, sizeof(rbuf) - rlen);
        if (got <= 0) {
            break;
        }
        rlen += got;
        if (parse_window_report(rbuf, rlen, rows, cols, &report_start, &report_end)) {
            found = 1;
            break;
        }
    }

    for (int i = 0; i < rlen && *pending_len < pending_size; i++) {
        if (found && i >= report_start && i < report_end) {
            continue;
        }
        pending[(*pending_len)++] = rbuf[i];
    }

    return found;
}

static int append_size_prefix(char *dest, size_t dest_size, int rows, int cols)
{
    char digits[16];

    sprintf(digits, "%d", rows);
    if (!host_append_literal(dest, dest_size, "stty rows ") ||
        !host_append_literal(dest, dest_size, digits)) {
        return 0;
    }
    sprintf(digits, "%d", cols);
    return host_append_literal(dest, dest_size, " cols ") &&
           host_append_literal(dest, dest_size, digits) &&
           host_append_literal(dest, dest_size, " 2>/dev/null; ");
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char session_command[HOST_MAX_COMMAND_LEN];
    static char pending[128];
    static char buffer[4096];
    BPTR in = 0;
    BPTR out = 0;
    long handle = 0;
    BOOL raw_mode = FALSE;
    struct host_terminal_filter terminal_filter = { HOST_TERMINAL_TEXT, 0 };
    long actual;
    ULONG status;
    int status_supported = 0;
    int return_code = 0;
    int term_rows = 0;
    int term_cols = 0;
    int pending_len = 0;

    command[0] = '\0';
    session_command[0] = '\0';

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

    /*
     * The host pty is created without a window size; pass the console
     * size through stty so full-screen host programs render correctly.
     */
    if (query_console_size(in, out, &term_rows, &term_cols,
                           pending, sizeof(pending), &pending_len) &&
        !append_size_prefix(session_command, sizeof(session_command), term_rows, term_cols)) {
        session_command[0] = '\0';
    }

    if (!host_append_shell_login_command(session_command, sizeof(session_command), command)) {
        printf("Command is too long\n");
        return_code = HOST_RETURN_ERROR;
        goto cleanup;
    }

    handle = HostShell_Open((UBYTE *)session_command);
    if (handle == 0) {
        printf("Failed to open host shell session.\n");
        return_code = HOST_RETURN_ERROR;
        goto cleanup;
    }

    if (pending_len > 0) {
        HostShell_Write(handle, (UBYTE *)pending, pending_len);
    }

    for (;;)
    {
        // Check for output from host
        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer) - 2);
        if (actual > 0)
        {
            int outptr = host_terminal_filter_process(&terminal_filter,
                                                      (const unsigned char *)buffer, actual,
                                                      (unsigned char *)outbuf, sizeof(outbuf));
            if (outptr < 0) {
                printf("Failed to translate host terminal output.\n");
                return_code = HOST_RETURN_ERROR;
                goto cleanup;
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
    if (out != 0) {
        int outptr = host_terminal_filter_finish(&terminal_filter,
                                                 (unsigned char *)outbuf, sizeof(outbuf));
        if (outptr > 0) {
            Write(out, outbuf, outptr);
        }
    }
    if (handle != 0) {
        HostShell_Close(handle);
    }
    if (raw_mode) {
        SetMode(in, 0); // Restore Cooked Mode
    }
    return return_code;
}
