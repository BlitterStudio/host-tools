/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include <dos/dosextens.h>

#include "host_amiga_file.h"
#include "host_capture.h"
#include "host_copy_command.h"

#define COPY_STALL_LIMIT 1500   /* ticks without a sign of life, only when HostShell_Status is missing */
#define COPY_SLICE 4096         /* largest HostShell_Write */
#define COPY_PROGRESS_STEP 32768

static const char version[] = "$VER: Host-Copy " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Copy v%s\n", VERSION_STR);
    printf("Host-Copy copies one file between a host path and the Amiga.\n");
    printf("%s\nUsage: host-copy GET <host path> <Amiga destination> [FORCE]\n", version);
    printf("       host-copy PUT <Amiga source> <host path> [FORCE]\n");
    return 0;
}

static int is_word_ci(const char *arg, const char *word)
{
    return host_has_prefix_ci(arg, word) && arg[strlen(word)] == '\0';
}

/* Progress for GET and PUT; the total is always known. */
static void show_progress(const char *label, unsigned long done, unsigned long total)
{
    unsigned long pct = 100;

    if (total > 0) {
        pct = done / ((total + 99) / 100);
        if (pct > 100) {
            pct = 100;
        }
    }
    printf("\r%s: %lu%% (%lu KB)", label, pct, done >> 10);
    fflush(stdout);
}

/* Tracks whether the host session still answers; shared by both directions. */
struct session_wait {
    int status_supported;
    int idle;
};

/*
 * Called when a read returned no data. There is no idle timeout while
 * the host reports the process as running. Hosts that do not implement
 * HostShell_Status never report that, so the stall limit applies there.
 */
static const char *session_idle(long handle, struct session_wait *wait)
{
    ULONG status = HostShell_Status(handle);

    if (status != HOST_SHELL_STATUS_INVALID) {
        wait->status_supported = 1;
    } else if (wait->status_supported) {
        return "Host session lost";
    }
    Delay(1);
    if (!wait->status_supported && ++wait->idle > COPY_STALL_LIMIT) {
        return "Timed out waiting for the host";
    }
    return NULL;
}

static int session_exited_ok(long handle, struct session_wait *wait)
{
    ULONG status = HostShell_Status(handle);

    if (status == HOST_SHELL_STATUS_INVALID) {
        return !wait->status_supported;
    }
    return (status & HOST_SHELL_STATUS_EXITED) != 0 && (status & 0xff) == 0;
}

static ULONG session_exit_code(long handle)
{
    ULONG status = HostShell_Status(handle);

    return (status & HOST_SHELL_STATUS_EXITED) != 0 ? (status & 0xff) : 0xffff;
}

static int ctrl_c_pressed(void)
{
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        return 1;
    }
    return 0;
}

/* ---- GET: host file to Amiga ---- */

static int do_get(const char *hostpath, const char *dest, int force, int windows)
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char destpath[HOST_MAX_PATH_LEN];
    static char temp_path[HOST_MAX_PATH_LEN];
    static char backup_path[HOST_MAX_PATH_LEN];
    static char buffer[4096];
    struct session_wait wait = { 0, 0 };
    long handle;
    long actual;
    int header_done = 0;
    int header_bad = 0;
    unsigned long expected = 0;
    unsigned long received = 0;
    unsigned long shown = (unsigned long)-1;
    BPTR out_file = 0;
    BPTR lock;
    int temp_exists = 0;
    int ended = 0;
    int rc = HOST_RETURN_ERROR;
    const char *failure = NULL;
    int isdir = 0;

    lock = host_quiet_lock(dest);
    if (lock) {
        isdir = host_lock_is_directory(lock);
        UnLock(lock);
    }

    destpath[0] = '\0';
    if (!host_append_literal(destpath, sizeof(destpath), dest)) {
        printf("Destination path is too long\n");
        return HOST_RETURN_ERROR;
    }
    if (isdir) {
        const char *name = host_copy_host_basename(hostpath, windows);

        if (!host_copy_amiga_name_ok(name)) {
            printf("Cannot use \"%s\" as an Amiga file name; give a file name in the destination\n", name);
            return HOST_RETURN_ERROR;
        }
        if (!AddPart((STRPTR)destpath, (STRPTR)name, sizeof(destpath))) {
            printf("Destination path is too long\n");
            return HOST_RETURN_ERROR;
        }
    }

    lock = host_quiet_lock(destpath);
    if (lock) {
        UnLock(lock);
        if (!force) {
            printf("Destination already exists (use FORCE to overwrite): %s\n", destpath);
            return HOST_RETURN_ERROR;
        }
    }

    command[0] = '\0';
    if (!(windows ? host_append_copy_get_command_windows(command, sizeof(command), hostpath)
                  : host_append_copy_get_command(command, sizeof(command), hostpath))) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    handle = HostShell_OpenPipe((UBYTE *)command);
    if (handle == 0) {
        printf("Failed to open a host session (host-copy needs an Amiberry build with pipe sessions)\n");
        return HOST_RETURN_ERROR;
    }

    for (;;) {
        if (ctrl_c_pressed()) {
            failure = "Aborted";
            break;
        }

        actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer));
        if (actual > 0) {
            long offset = 0;

            wait.idle = 0;

            /* the size line: digits, with blanks and carriage returns ignored */
            while (offset < actual && !header_done && !header_bad) {
                char c = buffer[offset++];

                if (c == '\n') {
                    header_done = 1;
                } else if (c >= '0' && c <= '9') {
                    if (expected > (HOST_COPY_MAX_SIZE - (unsigned long)(c - '0')) / 10) {
                        failure = "The host file is larger than 2 GB";
                        break;
                    }
                    expected = expected * 10 + (unsigned long)(c - '0');
                } else if (c != ' ' && c != '\t' && c != '\r') {
                    header_bad = 1;
                }
            }
            if (failure != NULL) {
                break;
            }
            if (header_bad) {
                failure = "Unexpected reply from the host";
                break;
            }
            if (header_done && out_file == 0) {
                if (!host_make_unused_sidecar_path(destpath, 'p', temp_path, sizeof(temp_path))) {
                    failure = "Cannot reserve a temporary destination file";
                    break;
                }
                out_file = Open((STRPTR)temp_path, MODE_NEWFILE);
                if (out_file == 0) {
                    failure = "Cannot open temporary destination file";
                    break;
                }
                temp_exists = 1;
            }
            if (offset < actual) {
                long data_len = actual - offset;

                if (received + (unsigned long)data_len > expected) {
                    failure = "The host file changed during the copy";
                    break;
                }
                if (Write(out_file, (APTR)(buffer + offset), data_len) != data_len) {
                    failure = "Write to destination failed";
                    break;
                }
                received += (unsigned long)data_len;
                if (shown == (unsigned long)-1 || received - shown >= COPY_PROGRESS_STEP) {
                    show_progress("Receiving", received, expected);
                    shown = received;
                }
            }
        } else if (actual < 0) {
            ended = 1;
            break;
        } else {
            failure = session_idle(handle, &wait);
            if (failure != NULL) {
                break;
            }
        }
    }

    if (failure == NULL && ended) {
        if (!header_done) {
            ULONG code = session_exit_code(handle);

            if (code == 2) {
                failure = "Host file not found, or not a regular file";
            } else if (code == 3) {
                failure = "Cannot read the host file";
            } else {
                failure = "The host did not answer (check that the host shell can run the command)";
            }
        } else if (!session_exited_ok(handle, &wait)) {
            failure = "Reading the host file failed";
        } else if (received != expected) {
            failure = "Copy is incomplete";
        }
    }
    HostShell_Close(handle);

    if (out_file != 0) {
        if (!Close(out_file) && failure == NULL) {
            failure = "Cannot finish temporary destination file";
        }
        out_file = 0;
    }

    if (failure == NULL) {
        if (host_install_file(temp_path, destpath, force, backup_path, sizeof(backup_path),
                              "Cannot move the copy to the destination",
                              "Destination appeared during the copy", &failure)) {
            temp_exists = 0;
        }
    }

    if (failure != NULL) {
        if (temp_exists && !DeleteFile((STRPTR)temp_path)) {
            printf("\nWarning: partial copy remains at %s\n", temp_path);
        }
        printf("\n%s: %s\n", failure, hostpath);
    } else {
        show_progress("Receiving", received, expected);
        printf("\nSaved %s (%lu bytes)\n", destpath, received);
        rc = 0;
    }

    return rc;
}

/* ---- PUT: Amiga file to host ---- */

/* Host output is a few short lines; a line longer than the buffer is dropped. */
struct reply_lines {
    char buf[256];
    int len;
};

static int reply_take_line(struct reply_lines *rl, char *line, size_t line_size)
{
    for (int i = 0; i < rl->len; i++) {
        if (rl->buf[i] == '\n') {
            size_t n = (size_t)i < line_size - 1 ? (size_t)i : line_size - 1;

            memcpy(line, rl->buf, n);
            line[n] = '\0';
            memmove(rl->buf, rl->buf + i + 1, (size_t)(rl->len - i - 1));
            rl->len -= i + 1;
            return 1;
        }
    }
    if (rl->len >= (int)sizeof(rl->buf)) {
        rl->len = 0;
    }
    return 0;
}

/* Read what the host has printed; returns the HostShell_Read result. */
static long reply_pump(long handle, struct reply_lines *rl)
{
    long got = HostShell_Read(handle, (UBYTE *)rl->buf + rl->len,
                              (ULONG)(sizeof(rl->buf) - (size_t)rl->len));

    if (got > 0) {
        rl->len += (int)got;
    }
    return got;
}

static int do_put(const char *srcpath, const char *hostpath, int force, int windows)
{
    static char command[HOST_MAX_COMMAND_LEN];
    static char name[128];
    static char line[128];
    static unsigned char filebuf[16384];
    static struct FileInfoBlock fib __attribute__((aligned(4)));
    struct session_wait wait = { 0, 0 };
    struct reply_lines replies;
    unsigned long total;
    unsigned long sent = 0;
    unsigned long shown = (unsigned long)-1;
    const char *code = NULL;
    const char *failure = NULL;
    int ready = 0;
    int confirmed = 0;
    BPTR lock;
    BPTR in_file;
    long handle;
    int rc = HOST_RETURN_ERROR;

    replies.len = 0;

    lock = host_quiet_lock(srcpath);
    if (lock == 0) {
        printf("Cannot find source file: %s\n", srcpath);
        return HOST_RETURN_ERROR;
    }
    if (!Examine(lock, &fib)) {
        UnLock(lock);
        printf("Cannot examine source file: %s\n", srcpath);
        return HOST_RETURN_ERROR;
    }
    UnLock(lock);
    if (fib.fib_DirEntryType > 0) {
        printf("Source is a directory; host-copy copies single files: %s\n", srcpath);
        return HOST_RETURN_ERROR;
    }
    total = (unsigned long)fib.fib_Size;

    /* The name is only used when the host path is an existing folder. */
    name[0] = '\0';
    if (host_copy_host_name_ok((const char *)fib.fib_FileName, windows) &&
        strlen((const char *)fib.fib_FileName) < sizeof(name)) {
        strcpy(name, (const char *)fib.fib_FileName);
    }

    command[0] = '\0';
    if (!(windows ? host_append_copy_put_command_windows(command, sizeof(command), hostpath, name,
                                                         total, force)
                  : host_append_copy_put_command(command, sizeof(command), hostpath, name,
                                                 total, force))) {
        printf("Command is too long\n");
        return HOST_RETURN_ERROR;
    }

    in_file = Open((STRPTR)srcpath, MODE_OLDFILE);
    if (in_file == 0) {
        printf("Cannot open source file: %s\n", srcpath);
        return HOST_RETURN_ERROR;
    }

    handle = HostShell_OpenPipe((UBYTE *)command);
    if (handle == 0) {
        Close(in_file);
        printf("Failed to open a host session (host-copy needs an Amiberry build with pipe sessions)\n");
        return HOST_RETURN_ERROR;
    }

    /* wait for the host to accept the target */
    while (failure == NULL && !ready) {
        long got;

        if (ctrl_c_pressed()) {
            failure = "Aborted";
            break;
        }
        if (reply_take_line(&replies, line, sizeof(line))) {
            enum host_copy_put_event event = host_copy_put_event_for(line, 0, total, &code);

            if (event == HOST_COPY_PUT_READY) {
                ready = 1;
            } else if (event == HOST_COPY_PUT_ERROR) {
                failure = host_copy_error_text(code);
            } else {
                failure = "Unexpected reply from the host";
            }
            continue;
        }
        got = reply_pump(handle, &replies);
        if (got > 0) {
            wait.idle = 0;
        } else if (got < 0) {
            failure = "The host did not answer (check that the host shell can run the command)";
        } else {
            failure = session_idle(handle, &wait);
        }
    }

    /* stream the file; the host reads exactly total bytes */
    while (failure == NULL && sent < total) {
        long n = Read(in_file, filebuf, sizeof(filebuf));
        long off = 0;

        if (n <= 0) {
            failure = "Cannot read the source file";
            break;
        }
        if ((unsigned long)n > total - sent) {
            n = (long)(total - sent);
        }
        while (off < n) {
            long slice = n - off;
            long w;

            if (ctrl_c_pressed()) {
                failure = "Aborted";
                break;
            }
            if (slice > COPY_SLICE) {
                slice = COPY_SLICE;
            }
            w = HostShell_Write(handle, filebuf + off, (ULONG)slice);
            if (w < 0) {
                /* the host stopped reading; its last line says why */
                for (int tries = 0; tries < 50 && failure == NULL; tries++) {
                    long got = reply_pump(handle, &replies);

                    if (reply_take_line(&replies, line, sizeof(line)) &&
                        host_copy_put_event_for(line, sent, total, &code) == HOST_COPY_PUT_ERROR) {
                        failure = host_copy_error_text(code);
                    } else if (got < 0) {
                        break;
                    } else if (got == 0) {
                        Delay(1);
                    }
                }
                if (failure == NULL) {
                    failure = "The host stopped reading the file";
                }
                break;
            }
            if (w == 0) {
                Delay(1);
                continue;
            }
            off += w;
            sent += (unsigned long)w;
        }
        if (failure != NULL) {
            break;
        }

        /*
         * Before the end of the file an answer can only be an error. After
         * the last write it may already be the final confirmation.
         */
        reply_pump(handle, &replies);
        if (reply_take_line(&replies, line, sizeof(line))) {
            enum host_copy_put_event event = host_copy_put_event_for(line, sent, total, &code);

            if (event == HOST_COPY_PUT_CONFIRMED) {
                confirmed = 1;
            } else if (event == HOST_COPY_PUT_ERROR) {
                failure = host_copy_error_text(code);
            } else {
                failure = "Unexpected reply from the host";
            }
            break;
        }
        if (shown == (unsigned long)-1 || sent - shown >= COPY_PROGRESS_STEP) {
            show_progress("Sending", sent, total);
            shown = sent;
        }
    }

    /* the host confirms the stored size */
    wait.idle = 0;
    while (failure == NULL && !confirmed) {
        long got;

        if (ctrl_c_pressed()) {
            failure = "Aborted";
            break;
        }
        if (reply_take_line(&replies, line, sizeof(line))) {
            enum host_copy_put_event event = host_copy_put_event_for(line, sent, total, &code);

            if (event == HOST_COPY_PUT_CONFIRMED) {
                confirmed = 1;
            } else if (event == HOST_COPY_PUT_ERROR) {
                failure = host_copy_error_text(code);
            } else {
                failure = "The host did not confirm the size of the file";
            }
            continue;
        }
        got = reply_pump(handle, &replies);
        if (got > 0) {
            wait.idle = 0;
        } else if (got < 0) {
            failure = "The host ended without confirming the file";
        } else {
            failure = session_idle(handle, &wait);
        }
    }

    HostShell_Close(handle);
    Close(in_file);

    if (failure != NULL) {
        printf("\n%s: %s\n", failure, hostpath);
    } else {
        show_progress("Sending", sent, total);
        printf("\nCopied %s to %s (%lu bytes)\n", srcpath, hostpath, total);
        rc = 0;
    }
    return rc;
}

int main(int argc, char *argv[])
{
    const char *args[3];
    int nargs = 0;
    int force = 0;
    int get;

    if (argc == 2 && strcmp(argv[1], "?") == 0) {
        return print_usage();
    }

    for (int i = 1; i < argc; i++) {
        if (is_word_ci(argv[i], "force")) {
            force = 1;
        } else if (nargs < 3) {
            args[nargs++] = argv[i];
        } else {
            printf("Unexpected argument: %s\n", argv[i]);
            print_usage();
            return HOST_RETURN_ERROR;
        }
    }

    if (nargs != 3 || (!is_word_ci(args[0], "get") && !is_word_ci(args[0], "put"))) {
        print_usage();
        return HOST_RETURN_ERROR;
    }
    get = is_word_ci(args[0], "get");

    if (!InitUAEResource()) {
        printf("UAEResource not found!\n");
        return 2;
    }

    {
        int windows = GetHostPlatform() == HOST_PLATFORM_WINDOWS;

        return get ? do_get(args[1], args[2], force, windows)
                   : do_put(args[1], args[2], force, windows);
    }
}
