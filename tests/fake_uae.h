/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * A stand-in for the parts of Amiberry that host-config and host-disk use,
 * written from Amiberry's source (cfgfile.cpp: cfgfile_uaelib_modify,
 * cfgfile_modify, cfgfile_searchconfig, cmdlineparser, execcmdline;
 * uaelib.cpp: InsertDisk and GetUaeConfig; disk.cpp: DISK_vsync and
 * disk_insert_2). The real src/uae_pragmas.h and src/host_path.h run against
 * it: the tools call a fake "calltrap" through uae.resource exactly as on an
 * Amiga.
 *
 * Time moves only in Delay(): one tick is one frame of the fake emulation.
 */

#ifndef FAKE_UAE_H
#define FAKE_UAE_H

/* dup, dup2, fileno and strdup are POSIX, not C99. */
#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amiga_stub/amiga_stub.h"
#include "uae_pragmas.h"

#define FAKE static __attribute__((unused))

#define FAKE_DRIVES 4
#define FAKE_NAME_MAX 256
#define FAKE_STORE_MAX 8192
#define FAKE_ARGV_MAX 100
#define FAKE_SET_LOG 8
#define FAKE_LINE_MAX 2048
#define FAKE_EJECT_FRAMES 100

struct fake_drive {
    int type;                    /* floppyNtype; -1 is no drive */
    char df[FAKE_NAME_MAX];      /* currprefs.floppyslots[n].df */
    char pending[FAKE_NAME_MAX]; /* changed_prefs.floppyslots[n].df */
    int in;                      /* disk_in_dfN */
    int wait;                    /* frames until the delayed insert */
};

struct fake_option {
    const char *name;
    char value[64];
    char pending[64];
    int has_pending;
};

struct fake_file {
    const char *amiga; /* path the Amiga knows */
    const char *host;  /* its host path, NULL when Amiberry cannot map it */
};

/* Host files that load as disk images. */
static const char *const fake_host_images[] = {
    "/host/disks/good.adf",
    "/host/disks/with space.adf",
    NULL
};

static struct fake_file fake_files[] = {
    { "Work:disks/good.adf", "/host/disks/good.adf" },
    { "Work:disks/with space.adf", "/host/disks/with space.adf" },
    { "Work:disks/bad.adf", "/host/disks/bad.adf" },
    { "Work:disks", "/host/disks" },
    { "max", "/host/max" },
    { "DH9:hardfile.adf", NULL },
    { NULL, NULL }
};

static struct fake_option fake_options[] = {
    { "cpu_speed", "real", "", 0 },
    { "chipset", "aga", "", 0 },
    { "floppy_speed", "100", "", 0 },
    { "kickstart_rom_file", "/roms/kick.rom", "", 0 },
    { NULL, "", "", 0 }
};

static struct fake_drive fake_drives[FAKE_DRIVES];
static int fake_resource_present;
static int fake_break_pending;
static long fake_frames;
static int fake_insert_calls;
static int fake_insert_drive;
static char fake_insert_name[512];
static int fake_eject_calls;
static int fake_set_count;
static char fake_set_lines[FAKE_SET_LOG][FAKE_LINE_MAX];

/* The configuration text Amiberry searches: a snapshot of currprefs. */
static char fake_store[FAKE_STORE_MAX];
static size_t fake_store_len;
static size_t fake_store_pos;
static int fake_store_valid;
static char fake_search[128];
static int fake_have_search;

FAKE void fake_reset(void)
{
    memset(fake_drives, 0, sizeof(fake_drives));
    fake_drives[0].type = 0;
    fake_drives[1].type = 0;
    fake_drives[2].type = -1;
    fake_drives[3].type = -1;
    for (int i = 0; fake_options[i].name; i++) {
        fake_options[i].has_pending = 0;
    }
    strcpy(fake_options[0].value, "real");
    strcpy(fake_options[1].value, "aga");
    strcpy(fake_options[2].value, "100");
    fake_resource_present = 1;
    fake_break_pending = 0;
    fake_frames = 0;
    fake_insert_calls = 0;
    fake_insert_drive = -1;
    fake_insert_name[0] = '\0';
    fake_eject_calls = 0;
    fake_set_count = 0;
    fake_store_valid = 0;
    fake_have_search = 0;
}

FAKE int fake_image_loads(const char *path)
{
    for (int i = 0; fake_host_images[i]; i++) {
        if (strcmp(fake_host_images[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

/* DISK_vsync: act on a changed path, with the disk change delay when a disk is in. */
FAKE void fake_frame(void)
{
    fake_frames++;
    for (int i = 0; i < FAKE_DRIVES; i++) {
        struct fake_drive *drive = &fake_drives[i];

        if (drive->wait > 0 && --drive->wait == 0) {
            drive->in = fake_image_loads(drive->df);
        }
        if (strcmp(drive->df, drive->pending) != 0) {
            strcpy(drive->df, drive->pending);
            if (drive->df[0] == '\0') {
                drive->in = 0;
                drive->wait = 0;
            } else if (drive->in) {
                drive->in = 0;
                drive->wait = FAKE_EJECT_FRAMES;
            } else {
                drive->wait = 1;
            }
        }
    }
    for (int i = 0; fake_options[i].name; i++) {
        if (fake_options[i].has_pending) {
            strcpy(fake_options[i].value, fake_options[i].pending);
            fake_options[i].has_pending = 0;
        }
    }
}

FAKE void fake_run_frames(long frames)
{
    while (frames-- > 0) {
        fake_frame();
    }
}

/* cfgfile_createconfigstore with uaeconfig set: every option, defaults included. */
FAKE void fake_create_store(void)
{
    char *p = fake_store;

    for (int i = 0; fake_options[i].name; i++) {
        p += sprintf(p, "%s=%s\n", fake_options[i].name, fake_options[i].value);
    }
    for (int i = 0; i < FAKE_DRIVES; i++) {
        p += sprintf(p, "floppy%d=%s\n", i, fake_drives[i].df);
        p += sprintf(p, "floppy%dtype=%d\n", i, fake_drives[i].type);
    }
    *p++ = '\0';
    fake_store_len = (size_t)(p - fake_store);
    fake_store_pos = 0;
    fake_store_valid = 1;
}

/* cfgfile_searchconfig */
FAKE int fake_searchconfig(const char *in, int index, char *out, int outsize)
{
    char tmp[2560];
    int j = 0;
    int inlen = (int)strlen(in);
    int joker = 0;
    int err;
    int found = 0;
    int index2 = index;

    if (in[inlen - 1] == '*') {
        joker = 1;
        inlen--;
    }
    *out = '\0';

    if (index < 0) {
        index = 0;
        fake_store_pos = 0;
    } else if (fake_store_pos > 0) {
        index = 0;
    }

    tmp[0] = '\0';
    for (;;) {
        unsigned char b = 0;

        if (fake_store_pos < fake_store_len) {
            b = (unsigned char)fake_store[fake_store_pos++];
        }
        if (b == 0) {
            err = 10;
            if (found || index2 > 0) {
                err = 0;
            }
            return err;
        }
        if (b == '\n') {
            if (strncmp(tmp, in, (size_t)inlen) == 0 &&
                ((inlen > 0 && (int)strlen(tmp) > inlen && tmp[inlen] == '=') || joker)) {
                if (index <= 0) {
                    const char *p = joker ? tmp : strchr(tmp, '=') + 1;

                    strncpy(out, p, (size_t)outsize - 1);
                    out[outsize - 1] = '\0';
                    found = 1;
                    return -1;
                }
                index--;
            }
            j = 0;
            tmp[0] = '\0';
        } else {
            tmp[j++] = (char)b;
            tmp[j] = '\0';
        }
    }
}

/* cmdlineparser: spaces split tokens, quotes group, control characters vanish, empty tokens are lost. */
FAKE int fake_cmdlineparser(const char *s, char *argv[], int max)
{
    int cnt = 0;
    int quote = 0;
    int doout;
    int j = 0;
    char tmp1[2048];

    tmp1[0] = '\0';
    argv[0] = NULL;
    while (cnt < max) {
        char c = *s++;

        if (!c) {
            break;
        }
        if ((unsigned char)c < 32) {
            continue;
        }
        doout = 0;
        if (c == '"') {
            if (quote) {
                quote = 0;
                doout = 1;
            } else {
                quote = 1;
                j = -1;
            }
        }
        if (!quote && c == ' ') {
            doout = 1;
        }
        if (!doout) {
            if (j >= 0) {
                tmp1[j] = c;
                tmp1[j + 1] = '\0';
            }
            j++;
        } else {
            if (strlen(tmp1) > 0) {
                argv[cnt++] = strdup(tmp1);
                argv[cnt] = NULL;
            }
            tmp1[0] = '\0';
            j = 0;
        }
    }
    if (j > 0 && cnt < max) {
        argv[cnt++] = strdup(tmp1);
        argv[cnt] = NULL;
    }
    return cnt;
}

/* execcmdline: pairs of option and value; an unknown option stops with code 5. */
FAKE int fake_execcmdline(int argc, char *argv[])
{
    for (int i = 0; i < argc; i++) {
        if (i + 2 <= argc) {
            const char *option = argv[i];
            const char *value = argv[i + 1];
            int known = 0;

            for (int d = 0; d < FAKE_DRIVES; d++) {
                char name[16];

                sprintf(name, "floppy%d", d);
                if (strcmp(option, name) == 0) {
                    strncpy(fake_drives[d].pending, value, FAKE_NAME_MAX - 1);
                    known = 1;
                }
            }
            for (int o = 0; !known && fake_options[o].name; o++) {
                if (strcmp(option, fake_options[o].name) == 0) {
                    strncpy(fake_options[o].pending, value, sizeof(fake_options[o].pending) - 1);
                    fake_options[o].has_pending = 1;
                    known = 1;
                }
            }
            if (!known) {
                return 5;
            }
            if (fake_set_count < FAKE_SET_LOG) {
                snprintf(fake_set_lines[fake_set_count], sizeof(fake_set_lines[0]), "%s=%s", option, value);
                fake_set_count++;
            }
            i++;
        }
    }
    return 0;
}

/* cfgfile_uaelib_modify and cfgfile_modify */
FAKE int fake_config_modify(int index, const char *parms, size_t size, char *out, size_t outsize)
{
    char line[2048];
    char *argv[FAKE_ARGV_MAX + 1];
    int argc;
    int err;

    if (outsize >= 32768) {
        return 0;
    }
    if (out && outsize > 0) {
        out[0] = '\0';
    }
    if (size == 0) {
        size = strlen(parms);
        if (size >= 32768) {
            return 0;
        }
    }
    if (size >= sizeof(line)) {
        return 10;
    }
    for (size_t i = 0; i < size; i++) {
        line[i] = parms[i];
        if (line[i] == 10 || line[i] == 13 || line[i] == 0) {
            size = i;
            break;
        }
    }
    line[size] = '\0';

    if (index != -1) {
        if (!fake_store_valid) {
            return 20;
        }
        if (fake_have_search) {
            return fake_searchconfig(fake_search, index, out, (int)outsize);
        }
        return -1;
    }

    argc = fake_cmdlineparser(line, argv, FAKE_ARGV_MAX);
    if (argc <= 1) {
        fake_create_store();
        fake_have_search = 0;
        if (argc > 0 && strlen(argv[0]) > 0) {
            strncpy(fake_search, argv[0], sizeof(fake_search) - 1);
            fake_have_search = 1;
        }
        err = -1;
    } else {
        err = fake_execcmdline(argc, argv);
    }
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    return err;
}

FAKE int fake_calltrap(int trap, ...)
{
    va_list ap;
    int result = 0;

    va_start(ap, trap);
    switch (trap) {
        case 1: {
            struct UAE_CONFIG *config = va_arg(ap, struct UAE_CONFIG *);
            char *names[FAKE_DRIVES] = { config->df0_name, config->df1_name, config->df2_name, config->df3_name };

            memset(config, 0, sizeof(*config));
            config->disk_in_df0 = (UBYTE)fake_drives[0].in;
            config->disk_in_df1 = (UBYTE)fake_drives[1].in;
            config->disk_in_df2 = (UBYTE)fake_drives[2].in;
            config->disk_in_df3 = (UBYTE)fake_drives[3].in;
            for (int i = 0; i < FAKE_DRIVES; i++) {
                strncpy(names[i], fake_drives[i].df, 255);
            }
            result = 1;
            break;
        }
        case 5: {
            const char *name = va_arg(ap, const char *);
            size_t drive = va_arg(ap, size_t);

            fake_insert_calls += name[0] ? 1 : 0;
            fake_eject_calls += name[0] ? 0 : 1;
            if (name[0]) {
                fake_insert_drive = (int)drive;
                snprintf(fake_insert_name, sizeof(fake_insert_name), "%s", name);
            }
            if (drive > 3 || strlen(name) >= FAKE_NAME_MAX) {
                result = 0;
            } else {
                strcpy(fake_drives[drive].pending, name);
                result = 1;
            }
            break;
        }
        case 82: {
            int mode = va_arg(ap, int);
            const char *parms = va_arg(ap, const char *);
            size_t size = va_arg(ap, size_t);
            size_t out = va_arg(ap, size_t);
            size_t outsize = va_arg(ap, size_t);

            result = fake_config_modify(mode, parms, size, (char *)out, outsize);
            break;
        }
        case 85: {
            size_t mode = va_arg(ap, size_t);
            size_t lock = va_arg(ap, size_t);
            char *out = (char *)va_arg(ap, size_t);
            size_t outsize = va_arg(ap, size_t);
            const char *host = fake_files[lock - 1].host;

            if (mode != 0 || host == NULL) {
                result = -1;
            } else {
                strncpy(out, host, outsize - 1);
                out[outsize - 1] = '\0';
                result = 0;
            }
            break;
        }
        default:
            result = 0;
            break;
    }
    va_end(ap);
    return result;
}

static struct uaebase fake_resource;

APTR OpenResource(UBYTE *name)
{
    (void)name;
    if (!fake_resource_present) {
        return NULL;
    }
    /* InitUAEResource adds 0xFF60 to uae_rombase to find the trap entry. */
    fake_resource.uae_rombase = (APTR)((uintptr_t)fake_calltrap - 0xFF60);
    return &fake_resource;
}

struct Task *FindTask(const char *name)
{
    static struct Process process;

    (void)name;
    return (struct Task *)&process;
}

ULONG SetSignal(ULONG new_signals, ULONG signal_set)
{
    ULONG old = fake_break_pending ? SIGBREAKF_CTRL_C : 0;

    (void)new_signals;
    if (signal_set & SIGBREAKF_CTRL_C) {
        fake_break_pending = 0;
    }
    return old;
}

BPTR Lock(STRPTR name, long mode)
{
    (void)mode;
    for (int i = 0; fake_files[i].amiga; i++) {
        if (strcmp(fake_files[i].amiga, (const char *)name) == 0) {
            return (BPTR)(i + 1);
        }
    }
    return 0;
}

void UnLock(BPTR lock)
{
    (void)lock;
}

void Delay(long ticks)
{
    fake_run_frames(ticks);
}

/* Runs fn with stdout redirected; returns the exit code and the output text. */
FAKE int fake_run(int (*fn)(int, char **), int argc, char **argv, char *text, size_t text_size)
{
    FILE *tmp = tmpfile();
    int saved;
    int rc;
    size_t got;

    fflush(stdout);
    saved = dup(1);
    dup2(fileno(tmp), 1);
    rc = fn(argc, argv);
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    rewind(tmp);
    got = fread(text, 1, text_size - 1, tmp);
    text[got] = '\0';
    fclose(tmp);
    return rc;
}

#endif
