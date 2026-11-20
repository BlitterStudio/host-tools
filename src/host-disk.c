/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_config_access.h"
#include "host_disk_command.h"
#include "host_path.h"

static const char version[] = "$VER: Host-Disk " VERSION_STR " (" DATE_STR ")";

/*
 * Inserting and ejecting set the drive's image path in Amiberry's pending
 * configuration (InsertDisk, trap 5). Amiberry acts on it at its next frame,
 * but only waits for a disk change delay of about two seconds when a disk is
 * already in the drive, and records the path whether or not the image can be
 * read. GetUaeConfig (trap 1) reports the running state: the image path and
 * whether the drive holds a disk, so the tool polls that to learn the result.
 */
#define POLL_TICKS 5
#define INSERT_POLLS 80
#define EJECT_POLLS 30

static struct UAE_CONFIG uae_config;

static int print_usage(void)
{
    printf("Host-Disk v%s\n", VERSION_STR);
    printf("Host-Disk lists, inserts and ejects floppy disk images in DF0: to DF3:.\n");
    printf("%s\nUsage: host-disk\n"
           "       host-disk insert <drive> <file>\n"
           "       host-disk eject <drive>\n"
           "Drive: 0-3 or DF0:-DF3:\n", version);
    return 0;
}

static const char *drive_image(int drive)
{
    switch (drive) {
        case 0: return uae_config.df0_name;
        case 1: return uae_config.df1_name;
        case 2: return uae_config.df2_name;
        default: return uae_config.df3_name;
    }
}

static int drive_has_disk(int drive)
{
    switch (drive) {
        case 0: return uae_config.disk_in_df0 != 0;
        case 1: return uae_config.disk_in_df1 != 0;
        case 2: return uae_config.disk_in_df2 != 0;
        default: return uae_config.disk_in_df3 != 0;
    }
}

static int read_state(void)
{
    if (!GetUaeConfig(&uae_config)) {
        printf("Amiberry could not report the state of the drives\n");
        return 0;
    }
    uae_config.df0_name[sizeof(uae_config.df0_name) - 1] = '\0';
    uae_config.df1_name[sizeof(uae_config.df1_name) - 1] = '\0';
    uae_config.df2_name[sizeof(uae_config.df2_name) - 1] = '\0';
    uae_config.df3_name[sizeof(uae_config.df3_name) - 1] = '\0';
    return 1;
}

/* A drive exists when its type option is not negative. When the option cannot be read, assume it does. */
static int drive_exists(int drive)
{
    static char reply[HOST_CONFIG_REPLY_MAX];
    char option[24];
    long type;
    long rc;

    if (!host_disk_type_option(drive, option, sizeof(option))) {
        return 1;
    }
    if (host_config_get(option, reply, sizeof(reply), &rc) != HOST_CONFIG_FOUND) {
        return 1;
    }
    if (!host_config_parse_int(reply, &type)) {
        return 1;
    }
    return type >= 0;
}

/* Waits one poll interval. Returns 1 when the user pressed Ctrl-C. */
static int wait_interrupted(void)
{
    Delay(POLL_TICKS);
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
        SetSignal(0, SIGBREAKF_CTRL_C);
        printf("***Break\n");
        return 1;
    }
    return 0;
}

static int list_drives(void)
{
    char line[HOST_DISK_PATH_MAX + 48];

    if (!read_state()) {
        return HOST_RETURN_ERROR;
    }
    for (int drive = 0; drive < HOST_DISK_DRIVES; drive++) {
        host_disk_format_line(line, sizeof(line), drive, drive_exists(drive),
                              drive_has_disk(drive), drive_image(drive));
        printf("%s\n", line);
    }
    return 0;
}

static int insert_disk(int drive, const char *file)
{
    static char host_path[HOST_MAX_PATH_LEN];
    const char *target;
    int status;

    if (!drive_exists(drive)) {
        printf("DF%d: does not exist in the current configuration\n", drive);
        return HOST_RETURN_ERROR;
    }

    status = host_resolve_optional_path(file, host_path, sizeof(host_path), &target);
    if (status != HOST_PATH_OK) {
        printf("%s: %s\n", host_path_error(status), file);
        if (status == HOST_PATH_NATIVE_ERROR) {
            printf("The file must be on a volume that Amiberry maps to a host directory.\n");
        }
        return HOST_RETURN_ERROR;
    }
    if (target[0] == '\0') {
        printf("Missing file name\n");
        return HOST_RETURN_ERROR;
    }
    if (strlen(target) > HOST_DISK_PATH_MAX) {
        printf("Path is too long for Amiberry (limit %d characters): %s\n", HOST_DISK_PATH_MAX, target);
        return HOST_RETURN_ERROR;
    }

    if (!InsertDisk((UBYTE *)target, (ULONG)drive)) {
        printf("Amiberry did not accept the disk image for DF%d:\n", drive);
        return HOST_RETURN_ERROR;
    }

    for (int poll = 0; poll < INSERT_POLLS; poll++) {
        if (!read_state()) {
            return HOST_RETURN_ERROR;
        }
        if (drive_has_disk(drive) && strcmp(drive_image(drive), target) == 0) {
            printf("Inserted %s in DF%d:\n", target, drive);
            return 0;
        }
        if (wait_interrupted()) {
            return HOST_RETURN_ERROR;
        }
    }

    /* The failed path stays in the configuration; clear it so DF<n>: reads as empty. */
    EjectDisk((ULONG)drive);
    printf("Amiberry could not load %s into DF%d:. Check that the file exists on the host and is a disk image Amiberry can read.\n",
           target, drive);
    return HOST_RETURN_ERROR;
}

static int eject_disk(int drive)
{
    if (!drive_exists(drive)) {
        printf("DF%d: does not exist in the current configuration\n", drive);
        return HOST_RETURN_ERROR;
    }

    if (!EjectDisk((ULONG)drive)) {
        printf("Amiberry did not eject DF%d:\n", drive);
        return HOST_RETURN_ERROR;
    }

    for (int poll = 0; poll < EJECT_POLLS; poll++) {
        if (!read_state()) {
            return HOST_RETURN_ERROR;
        }
        if (!drive_has_disk(drive) && drive_image(drive)[0] == '\0') {
            printf("Ejected DF%d:\n", drive);
            return 0;
        }
        if (wait_interrupted()) {
            return HOST_RETURN_ERROR;
        }
    }

    printf("DF%d: still holds a disk\n", drive);
    return HOST_RETURN_ERROR;
}

static int require_drive(const char *text)
{
    int drive = host_disk_parse_drive(text);

    if (drive < 0) {
        printf("Invalid drive: %s (use 0-3 or DF0:-DF3:)\n", text);
    }
    return drive;
}

int main(int argc, char *argv[])
{
    int drive;

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        return list_drives();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (strcmp(argv[1], "insert") == 0) {
        if (argc != 4) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        drive = require_drive(argv[2]);
        if (drive < 0) {
            return HOST_RETURN_ERROR;
        }
        return insert_disk(drive, argv[3]);
    }

    if (strcmp(argv[1], "eject") == 0) {
        if (argc != 3) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        drive = require_drive(argv[2]);
        if (drive < 0) {
            return HOST_RETURN_ERROR;
        }
        return eject_disk(drive);
    }

    printf("Unknown disk command\n");
    print_usage();
    return HOST_RETURN_ERROR;
}
