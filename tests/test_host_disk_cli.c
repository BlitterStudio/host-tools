/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fake_uae.h"

#define main host_disk_main
#include "../src/host-disk.c"
#undef main

static char text[8192];

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\noutput: %s\n", message, text);
        exit(1);
    }
}

static int run_args(char *a1, char *a2, char *a3, char *a4)
{
    char *argv[] = { "host-disk", a1, a2, a3, a4, NULL };
    int argc = 1 + (a1 != NULL) + (a2 != NULL) + (a3 != NULL) + (a4 != NULL);

    int rc = fake_run(host_disk_main, argc, argv, text, sizeof(text));

    require(fake_sem_depth == 0, "the search semaphore is released when the tool returns");
    return rc;
}

static void test_no_resource(void)
{
    fake_reset();
    fake_resource_present = 0;
    require(run_args(NULL, NULL, NULL, NULL) == 2, "missing uae.resource returns 2");
    require(strstr(text, "UAEResource not found!") != NULL, "missing uae.resource is reported");
}

static void test_usage(void)
{
    fake_reset();
    require(run_args("?", NULL, NULL, NULL) == 0, "? succeeds");
    require(strstr(text, "$VER: Host-Disk") != NULL && strstr(text, "host-disk insert <drive> <file>") != NULL,
            "usage shows the version and the commands");
    require(run_args("frobnicate", NULL, NULL, NULL) == HOST_RETURN_ERROR, "unknown command");
    require(run_args("insert", "0", NULL, NULL) == HOST_RETURN_ERROR, "insert needs a file");
    require(run_args("insert", "0", "a", "b") == HOST_RETURN_ERROR, "insert takes one file");
    require(run_args("eject", NULL, NULL, NULL) == HOST_RETURN_ERROR, "eject needs a drive");
    require(run_args("eject", "0", "1", NULL) == HOST_RETURN_ERROR, "eject takes one drive");
    require(run_args("insert", "DF4:", "Work:disks/good.adf", NULL) == HOST_RETURN_ERROR, "drive out of range");
    require(strstr(text, "Invalid drive: DF4:") != NULL, "bad drive is explained");
    require(run_args("eject", "x", NULL, NULL) == HOST_RETURN_ERROR, "drive that is not a drive");
    require(fake_insert_calls == 0 && fake_eject_calls == 0, "usage errors touch nothing");
}

static void test_list(void)
{
    fake_reset();
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list succeeds");
    require(strcmp(text, "DF0: (empty)\nDF1: (empty)\nDF2: (no drive)\nDF3: (no drive)\n") == 0,
            "list shows empty drives and missing drives");

    strcpy(fake_drives[1].df, "/host/disks/good.adf");
    strcpy(fake_drives[1].pending, "/host/disks/good.adf");
    fake_drives[1].in = 1;
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list with a disk");
    require(strcmp(text, "DF0: (empty)\nDF1: /host/disks/good.adf\nDF2: (no drive)\nDF3: (no drive)\n") == 0,
            "list shows the image in a drive");

    fake_drives[2].type = 0;
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list with a third drive");
    require(strstr(text, "DF2: (empty)\n") != NULL, "a drive added to the configuration is listed as empty");
}

static void test_insert(void)
{
    fake_reset();
    require(run_args("insert", "DF0:", "Work:disks/good.adf", NULL) == 0, "insert succeeds");
    require(strcmp(text, "Inserted /host/disks/good.adf in DF0:\n") == 0, "insert reports the host path");
    require(fake_insert_drive == 0 && strcmp(fake_insert_name, "/host/disks/good.adf") == 0,
            "Amiberry received the translated host path");
    require(fake_frames < FAKE_EJECT_FRAMES, "an empty drive needs no disk change delay");

    require(run_args(NULL, NULL, NULL, NULL) == 0, "list after insert");
    require(strstr(text, "DF0: /host/disks/good.adf\n") != NULL, "the drive lists the image");
}

static void test_insert_spaces_and_drive_forms(void)
{
    fake_reset();
    require(run_args("insert", "1", "Work:disks/with space.adf", NULL) == 0, "drive given as a digit");
    require(fake_insert_drive == 1 && strcmp(fake_insert_name, "/host/disks/with space.adf") == 0,
            "a path with a space is passed whole");
    require(fake_drives[1].in == 1, "disk is in DF1:");

    fake_reset();
    require(run_args("insert", "df1", "Work:disks/good.adf", NULL) == 0, "drive given as df1");
    require(fake_insert_drive == 1, "df1 selects DF1:");
}

static void test_insert_replaces_disk(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "first insert");
    fake_frames = 0;
    require(run_args("insert", "0", "Work:disks/with space.adf", NULL) == 0, "replace the disk");
    require(fake_frames >= FAKE_EJECT_FRAMES, "waits for the disk change delay instead of reporting the old disk");
    require(strstr(text, "Inserted /host/disks/with space.adf in DF0:") != NULL, "replacement is reported");
    require(strcmp(fake_drives[0].df, "/host/disks/with space.adf") == 0 && fake_drives[0].in == 1,
            "the new image is in the drive");

    fake_frames = 0;
    require(run_args("insert", "0", "Work:disks/with space.adf", NULL) == 0, "insert the same image again");
    require(fake_frames < 10, "the same image is reported at once");
}

/*
 * Amiberry takes the new path over at once and ejects the old disk a frame
 * later. A poll in between sees the new path with the old disk; it must not
 * count as a successful insert.
 */
static void test_replace_with_unreadable_image(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "first insert");

    require(run_args("insert", "0", "Work:disks/bad.adf", NULL) == HOST_RETURN_ERROR,
            "an unreadable image does not replace a good disk");
    require(strstr(text, "could not load /host/disks/bad.adf into DF0:") != NULL, "the failure is reported");
    require(strstr(text, "Inserted") == NULL, "no success is reported");
    fake_run_frames(5);
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list after the failed replacement");
    require(strncmp(text, "DF0: (empty)\n", 13) == 0, "the drive is left empty");
}

static void test_fake_shows_old_disk_with_new_name(void)
{
    /* the hazard the tool has to cope with is part of the model */
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "insert");
    require(InsertDisk((UBYTE *)"/host/disks/with space.adf", 0) == 1, "InsertDisk");
    fake_run_frames(1);
    require(strcmp(fake_drives[0].df, "/host/disks/with space.adf") == 0 && fake_drives[0].in == 1,
            "after one frame the drive reports the new path with the old disk");
    fake_run_frames(1);
    require(fake_drives[0].in == 0, "the old disk leaves a frame later");
}

static void test_insert_after_failed_load(void)
{
    /* the failed path is still set and nothing is in the drive */
    fake_reset();
    strcpy(fake_drives[0].df, "/host/disks/good.adf");
    strcpy(fake_drives[0].pending, "/host/disks/good.adf");
    fake_drives[0].in = 0;
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "insert the same path again");
    require(fake_drives[0].in == 1, "the disk is in after the retry");
    require(fake_eject_calls >= 1, "the stale path was cleared first so Amiberry sees a change");
}

static void test_replace_waits_for_change_delay(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "first insert");
    fake_frames = 0;
    require(run_args("insert", "0", "Work:disks/with space.adf", NULL) == 0, "replace");
    require(fake_frames >= 100, "the new disk goes in after the disk change delay");
    require(fake_eject_calls == 1, "the old disk is ejected first");
}

static void test_search_semaphore(void)
{
    fake_reset();
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list takes and releases the semaphore");
    require(fake_named_semaphore != NULL, "list creates the named semaphore");
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "insert under the semaphore");
    require(run_args("eject", "0", NULL, NULL) == 0, "eject under the semaphore");

    fake_reset();
    fake_sem_other_holds = 1;
    require(run_args(NULL, NULL, NULL, NULL) == HOST_RETURN_ERROR, "list while another tool searches");
    require(strstr(text, "Another host tool is reading") != NULL, "list reports the busy configuration");
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == HOST_RETURN_ERROR, "insert while busy");
    require(strstr(text, "Another host tool is reading") != NULL, "insert reports the busy configuration");
    require(fake_insert_calls == 0, "insert sends nothing while it cannot check the drive");
    require(run_args("eject", "0", NULL, NULL) == HOST_RETURN_ERROR, "eject while busy");
    require(fake_eject_calls == 0, "eject sends nothing while it cannot check the drive");
}

static void test_insert_path_kinds(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/nofile.adf", NULL) == HOST_RETURN_ERROR, "missing file on a mounted volume");
    require(strstr(text, "path not found: Work:disks/nofile.adf") != NULL, "a typo on a known volume is an error");
    require(fake_insert_calls == 0, "nothing is sent for a missing Amiga file");

    fake_reset();
    require(run_args("insert", "0", "C:\\Disks\\a.adf", NULL) == HOST_RETURN_ERROR, "Windows host path that does not load");
    require(fake_insert_calls == 1 && strcmp(fake_insert_name, "C:\\Disks\\a.adf") == 0,
            "a Windows path goes to Amiberry as written");
}

static void test_insert_host_path(void)
{
    fake_reset();
    require(run_args("insert", "0", "/host/disks/good.adf", NULL) == 0, "host path that is no Amiga path");
    require(strcmp(fake_insert_name, "/host/disks/good.adf") == 0, "host path goes through unchanged");
}

static void test_insert_failures(void)
{
    char long_path[400];

    fake_reset();
    require(run_args("insert", "0", "Work:disks/bad.adf", NULL) == HOST_RETURN_ERROR, "unreadable image");
    require(strstr(text, "could not load /host/disks/bad.adf into DF0:") != NULL, "unreadable image is reported");
    require(fake_eject_calls == 1, "the failed path is cleared from the configuration");
    fake_run_frames(5);
    require(run_args(NULL, NULL, NULL, NULL) == 0, "list after a failed insert");
    require(strncmp(text, "DF0: (empty)\n", 13) == 0, "a failed insert leaves the drive empty");

    fake_reset();
    require(run_args("insert", "0", "/nowhere/missing.adf", NULL) == HOST_RETURN_ERROR, "missing host file");
    require(strstr(text, "could not load /nowhere/missing.adf") != NULL, "missing host file is reported");

    fake_reset();
    require(run_args("insert", "2", "Work:disks/good.adf", NULL) == HOST_RETURN_ERROR, "drive that does not exist");
    require(strstr(text, "DF2: does not exist in the current configuration") != NULL, "missing drive is explained");
    require(fake_insert_calls == 0, "nothing is sent for a missing drive");

    fake_reset();
    require(run_args("insert", "0", "DH9:hardfile.adf", NULL) == HOST_RETURN_ERROR, "image in an unmapped hardfile");
    require(strstr(text, "native path translation failed") != NULL, "unmapped path is explained");
    require(fake_insert_calls == 0, "nothing is sent for an unmapped path");

    fake_reset();
    memset(long_path, 'a', sizeof(long_path));
    long_path[0] = '/';
    long_path[300] = '\0';
    require(run_args("insert", "0", long_path, NULL) == HOST_RETURN_ERROR, "path longer than Amiberry keeps");
    require(strstr(text, "too long for Amiberry (limit 255") != NULL, "long path is explained");
    require(fake_insert_calls == 0, "nothing is sent for a long path");

    fake_reset();
    memset(long_path, 'a', sizeof(long_path));
    long_path[0] = '/';
    long_path[255] = '\0';
    require(run_args("insert", "0", long_path, NULL) == HOST_RETURN_ERROR, "255-character path is sent");
    require(fake_insert_calls == 1, "a path of exactly 255 characters is passed on");
}

static void test_insert_break(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "first insert");
    fake_break_pending = 1;
    require(run_args("insert", "0", "Work:disks/with space.adf", NULL) == HOST_RETURN_ERROR, "Ctrl-C while waiting");
    require(strstr(text, "***Break") != NULL, "Ctrl-C is acknowledged");
}

static void test_eject(void)
{
    fake_reset();
    require(run_args("insert", "0", "Work:disks/good.adf", NULL) == 0, "insert");
    require(run_args("eject", "DF0:", NULL, NULL) == 0, "eject succeeds");
    require(strcmp(text, "Ejected DF0:\n") == 0, "eject is reported");
    require(fake_drives[0].in == 0 && fake_drives[0].df[0] == '\0', "the drive is empty");

    require(run_args("eject", "DF0:", NULL, NULL) == 0, "eject an empty drive");

    fake_reset();
    require(run_args("eject", "DF3:", NULL, NULL) == HOST_RETURN_ERROR, "eject a drive that does not exist");
    require(strstr(text, "DF3: does not exist in the current configuration") != NULL, "missing drive is explained");
    require(fake_eject_calls == 0, "nothing is sent for a missing drive");
}

int main(void)
{
    test_no_resource();
    test_usage();
    test_list();
    test_insert();
    test_insert_spaces_and_drive_forms();
    test_insert_replaces_disk();
    test_replace_with_unreadable_image();
    test_fake_shows_old_disk_with_new_name();
    test_insert_after_failed_load();
    test_replace_waits_for_change_delay();
    test_search_semaphore();
    test_insert_path_kinds();
    test_insert_host_path();
    test_insert_failures();
    test_insert_break();
    test_eject();
    return 0;
}
