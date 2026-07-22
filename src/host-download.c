/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <dos/dosextens.h>

#include "host_base64.h"
#include "host_capture.h"
#include "host_download_command.h"

#define DOWNLOAD_STALL_LIMIT 1500 /* ticks without data */

static const char version[] = "$VER: Host-Download " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Download v%s\n", VERSION_STR);
    printf("Host-Download downloads a URL on the host and saves it to an Amiga path.\n");
    printf("%s\nUsage: host-download <URL> [<destination>] [FORCE]\n", version);
    return 0;
}

static int is_force_arg(const char *arg)
{
    return host_has_prefix_ci(arg, "force") && arg[5] == '\0';
}

/* Lock without "please insert volume" requesters */
static BPTR quiet_lock(const char *path)
{
    struct Process *proc = (struct Process *)FindTask(NULL);
    APTR old_window = proc->pr_WindowPtr;
    BPTR lock;

    proc->pr_WindowPtr = (APTR)-1;
    lock = Lock((STRPTR)path, ACCESS_READ);
    proc->pr_WindowPtr = old_window;
    return lock;
}

static int lock_is_directory(BPTR lock)
{
    static struct FileInfoBlock fib __attribute__((aligned(4)));

    if (!Examine(lock, &fib)) {
        return 0;
    }
    return fib.fib_DirEntryType > 0;
}

static int make_unused_sidecar_path(const char *destination, char kind,
                                    char *path, size_t path_size)
{
    unsigned long token = (unsigned long)FindTask(NULL);

    for (unsigned int attempt = 0; attempt < 100; attempt++) {
        BPTR lock;

        if (!host_download_sidecar_path(destination, kind, token, attempt,
                                        path, path_size)) {
            return 0;
        }
        lock = quiet_lock(path);
        if (lock == 0) {
            return 1;
        }
        UnLock(lock);
    }
    return 0;
}

static int install_download(const char *temp_path, const char *destpath, int force,
                            char *backup_path, size_t backup_size,
                            const char **failure)
{
    BPTR lock = quiet_lock(destpath);

    if (lock == 0) {
        if (Rename((STRPTR)temp_path, (STRPTR)destpath)) {
            return 1;
        }
        *failure = "Cannot move download to destination";
        return 0;
    }

    if (lock_is_directory(lock)) {
        UnLock(lock);
        *failure = "Destination path is a directory";
        return 0;
    }
    UnLock(lock);

    if (!force) {
        *failure = "Destination appeared during download";
        return 0;
    }
    if (!make_unused_sidecar_path(destpath, 'b', backup_path, backup_size)) {
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

static void show_progress(unsigned long received, long expected)
{
    if (expected > 0) {
        unsigned long pct = received / (((unsigned long)expected + 99) / 100);
        if (pct > 100) {
            pct = 100;
        }
        printf("\rDownloading: %lu%% (%lu KB)", pct, received >> 10);
    } else {
        printf("\rDownloading: %lu KB", received >> 10);
    }
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char destpath[HOST_MAX_PATH_LEN];
    static char temp_path[HOST_MAX_PATH_LEN];
    static char backup_path[HOST_MAX_PATH_LEN];
    static char urlname[108];
    static char buffer[4096];
    static unsigned char decoded[(sizeof(buffer) * 3) / 4 + 3];
    struct host_base64_state b64;
    const char *url = NULL;
    const char *dest = NULL;
    int force = 0;
    int b64mode = 0;
    long handle;
    long actual;
    ULONG status = HOST_SHELL_STATUS_INVALID;
    int status_supported = 0;
    int idle_count = 0;
    int header_done = 0;
    int header_negative = 0;
    long expected = 0;
    unsigned long received = 0;
    unsigned long shown = (unsigned long)-1;
    BPTR out_file = 0;
    BPTR lock;
    int temp_exists = 0;
    int rc = HOST_RETURN_ERROR;
    const char *failure = NULL;

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc == 2 && strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++) {
        if (is_force_arg(argv[i])) {
            force = 1;
        } else if (url == NULL) {
            url = argv[i];
        } else if (dest == NULL) {
            dest = argv[i];
        } else {
            printf("Unexpected argument: %s\n", argv[i]);
            print_usage();
            return HOST_RETURN_ERROR;
        }
    }

    if (url == NULL) {
        printf("Missing URL argument\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    if (!host_download_url_supported(url)) {
        printf("Unsupported URL scheme (use HTTP, HTTPS, FTP, or FTPS): %s\n", url);
        return HOST_RETURN_ERROR;
    }

    host_url_filename(url, urlname, sizeof(urlname));

    destpath[0] = '\0';
    if (dest == NULL) {
        if (!host_append_literal(destpath, sizeof(destpath), urlname)) {
            printf("Destination path is too long\n");
            return HOST_RETURN_ERROR;
        }
    } else {
        int isdir = 0;

        lock = quiet_lock(dest);
        if (lock) {
            isdir = lock_is_directory(lock);
            UnLock(lock);
        }

        if (!host_append_literal(destpath, sizeof(destpath), dest)) {
            printf("Destination path is too long\n");
            return HOST_RETURN_ERROR;
        }
        if (isdir && !AddPart((STRPTR)destpath, (STRPTR)urlname, sizeof(destpath))) {
            printf("Destination path is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

    lock = quiet_lock(destpath);
    if (lock) {
        UnLock(lock);
        if (!force) {
            printf("Destination already exists (use FORCE to overwrite): %s\n", destpath);
            return HOST_RETURN_ERROR;
        }
    }

    {
        int platform = GetHostPlatform();
        int built;

        command[0] = '\0';
        if (platform == HOST_PLATFORM_WINDOWS) {
            built = host_append_download_stream_command_windows(command, sizeof(command), url);
        } else {
            built = host_append_download_stream_command(command, sizeof(command), url);
        }
        if (!built) {
            printf("Command is too long\n");
            return HOST_RETURN_ERROR;
        }

        handle = HostShell_OpenPipe((UBYTE *)command);
        if (handle == 0 && platform != HOST_PLATFORM_WINDOWS) {
            /* older Amiberry: pty session, base64 protected transfer */
            command[0] = '\0';
            if (!host_append_download_b64_command(command, sizeof(command), url)) {
                printf("Command is too long\n");
                return HOST_RETURN_ERROR;
            }
            handle = HostShell_Open((UBYTE *)command);
            b64mode = 1;
        }
    }
    if (handle == 0) {
        printf("Failed to open host download session.\n");
        return HOST_RETURN_ERROR;
    }

    if (!make_unused_sidecar_path(destpath, 'p', temp_path, sizeof(temp_path))) {
        HostShell_Close(handle);
        printf("Cannot reserve a temporary destination file\n");
        return HOST_RETURN_ERROR;
    }
    out_file = Open((STRPTR)temp_path, MODE_NEWFILE);
    if (out_file == 0) {
        HostShell_Close(handle);
        printf("Cannot open temporary destination file\n");
        return HOST_RETURN_ERROR;
    }
    temp_exists = 1;

    host_base64_init(&b64);

    for (;;)
    {
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            failure = "Aborted";
            break;
        }

        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer));
        if (actual > 0)
        {
            long offset = 0;
            const unsigned char *data;
            long data_len;

            idle_count = 0;

            while (offset < actual && !header_done) {
                char c = buffer[offset++];
                if (c == '\n') {
                    header_done = 1;
                    if (header_negative) {
                        expected = 0;
                    }
                } else if (c >= '0' && c <= '9') {
                    expected = expected * 10 + (c - '0');
                } else if (c == '-') {
                    header_negative = 1;
                }
                /* spaces, tabs and carriage returns are ignored */
            }

            if (offset >= actual) {
                continue;
            }

            if (b64mode) {
                data_len = host_base64_feed(&b64, buffer + offset, actual - offset, decoded);
                if (data_len < 0) {
                    failure = "Transfer is corrupt";
                    break;
                }
                data = decoded;
            } else {
                data = (const unsigned char *)buffer + offset;
                data_len = actual - offset;
            }

            if (data_len > 0) {
                if (Write(out_file, (APTR)data, data_len) != data_len) {
                    failure = "Write to destination failed";
                    break;
                }
                received += (unsigned long)data_len;
                if (shown == (unsigned long)-1 || received - shown >= 32768) {
                    show_progress(received, expected);
                    shown = received;
                }
            }
        }
        else if (actual < 0)
        {
            status = HostShell_Status(handle);
            break;
        }
        else
        {
            status = HostShell_Status(handle);
            if (status != HOST_SHELL_STATUS_INVALID) {
                status_supported = 1;
            }
            if ((status & HOST_SHELL_STATUS_EXITED) != 0) {
                break;
            }
            if (status == HOST_SHELL_STATUS_INVALID && status_supported) {
                failure = "Host session lost";
                break;
            }
            Delay(1);
            if (++idle_count > DOWNLOAD_STALL_LIMIT) {
                failure = "Timed out waiting for download data";
                break;
            }
        }
    }

    HostShell_Close(handle);

    if (failure == NULL) {
        int exited_ok;

        if (status == HOST_SHELL_STATUS_INVALID) {
            exited_ok = !status_supported;
        } else {
            exited_ok = (status & HOST_SHELL_STATUS_EXITED) != 0 && (status & 0xff) == 0;
        }

        if (!header_done) {
            failure = "Download failed";
        } else if (!exited_ok) {
            failure = "Download failed on the host";
        } else if (expected > 0 && received != (unsigned long)expected) {
            failure = "Download is incomplete";
        } else if (b64mode && b64.nbits >= 6) {
            failure = "Transfer is corrupt";
        }
    }

    if (out_file != 0) {
        if (!Close(out_file) && failure == NULL) {
            failure = "Cannot finish temporary destination file";
        }
        out_file = 0;
    }

    if (failure == NULL) {
        if (install_download(temp_path, destpath, force,
                             backup_path, sizeof(backup_path), &failure)) {
            temp_exists = 0;
        }
    }

    if (failure != NULL) {
        if (temp_exists && !DeleteFile((STRPTR)temp_path)) {
            printf("\nWarning: partial download remains at %s\n", temp_path);
        }
        printf("\n%s: %s\n", failure, url);
    } else {
        show_progress(received, expected);
        printf("\nSaved %s (%lu bytes)\n", destpath, received);
        rc = 0;
    }

    return rc;
}
