/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_DISK_COMMAND_H
#define HOST_DISK_COMMAND_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define HOST_DISK_DRIVES 4
/* Amiberry keeps a floppy image path in 256 bytes including the terminator. */
#define HOST_DISK_PATH_MAX 255

static inline int host_disk_ascii_upper(int c)
{
    return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
}

/*
 * Accepts 0-3, DF0-DF3 and DF0:-DF3: in any case. Returns the drive number,
 * or -1 for anything else (DF4:, "DF0:file", an empty string).
 */
static inline int host_disk_parse_drive(const char *text)
{
    if (text[0] >= '0' && text[0] <= '3' && text[1] == '\0') {
        return text[0] - '0';
    }
    if (host_disk_ascii_upper((unsigned char)text[0]) == 'D' &&
        host_disk_ascii_upper((unsigned char)text[1]) == 'F' &&
        text[2] >= '0' && text[2] <= '3' &&
        (text[3] == '\0' || (text[3] == ':' && text[4] == '\0'))) {
        return text[2] - '0';
    }
    return -1;
}

/* The option that holds the drive type; a negative value means no drive. */
static inline int host_disk_type_option(int drive, char *dest, size_t size)
{
    int written;

    if (drive < 0 || drive >= HOST_DISK_DRIVES) {
        return 0;
    }
    written = snprintf(dest, size, "floppy%dtype", drive);
    return written > 0 && (size_t)written < size;
}

/*
 * One line of the drive listing:
 *   DF0: <image path>        disk inserted
 *   DF0: <image path> (not loaded)
 *                            an image is set but the drive has no disk yet
 *   DF1: (empty)
 *   DF2: (no drive)
 */
static inline void host_disk_format_line(char *dest, size_t size, int drive, int has_drive,
                                         int disk_in, const char *image)
{
    if (!has_drive) {
        snprintf(dest, size, "DF%d: (no drive)", drive);
    } else if (image[0] == '\0') {
        snprintf(dest, size, "DF%d: (empty)", drive);
    } else if (!disk_in) {
        snprintf(dest, size, "DF%d: %s (not loaded)", drive, image);
    } else {
        snprintf(dest, size, "DF%d: %s", drive, image);
    }
}

#endif
