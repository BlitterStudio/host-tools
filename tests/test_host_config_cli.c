/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "fake_uae.h"

#define main host_config_main
#include "../src/host-config.c"
#undef main

static char text[8192];

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\noutput: %s\n", message, text);
        exit(1);
    }
}

static int run(int argc, char **argv)
{
    return fake_run(host_config_main, argc, argv, text, sizeof(text));
}

static int run_args(char *a1, char *a2, char *a3, char *a4)
{
    char *argv[] = { "host-config", a1, a2, a3, a4, NULL };
    int argc = 1 + (a1 != NULL) + (a2 != NULL) + (a3 != NULL) + (a4 != NULL);

    return run(argc, argv);
}

static void test_no_resource(void)
{
    fake_reset();
    fake_resource_present = 0;
    require(run_args("get", "cpu_speed", NULL, NULL) == 2, "missing uae.resource returns 2");
    require(strstr(text, "UAEResource not found!") != NULL, "missing uae.resource is reported");
}

static void test_usage(void)
{
    fake_reset();
    require(run_args(NULL, NULL, NULL, NULL) == HOST_RETURN_ERROR, "no command is an error");
    require(strstr(text, "Missing config command") != NULL, "no command is explained");
    require(run_args("?", NULL, NULL, NULL) == 0, "? succeeds");
    require(strstr(text, "$VER: Host-Config") != NULL && strstr(text, "host-config set <option> <value...>") != NULL,
            "usage shows the version and the commands");
    require(run_args("frobnicate", NULL, NULL, NULL) == HOST_RETURN_ERROR, "unknown command");
    require(run_args("get", NULL, NULL, NULL) == HOST_RETURN_ERROR, "get needs an option");
    require(run_args("get", "a", "b", NULL) == HOST_RETURN_ERROR, "get takes one option");
    require(run_args("set", "cpu_speed", NULL, NULL) == HOST_RETURN_ERROR, "set needs a value");
    require(strstr(text, "Usage error") != NULL, "set without a value is a usage error");
    require(fake_set_count == 0, "usage errors change nothing");
}

static void test_get(void)
{
    fake_reset();
    require(run_args("get", "cpu_speed", NULL, NULL) == 0, "get succeeds");
    require(strcmp(text, "real\n") == 0, "get prints just the value");

    require(run_args("get", "floppy2type", NULL, NULL) == 0, "get a negative number");
    require(strcmp(text, "-1\n") == 0, "negative value");

    require(run_args("get", "floppy0", NULL, NULL) == 0, "get an empty value");
    require(strcmp(text, "\n") == 0, "an option with an empty value prints an empty line");

    require(run_args("get", "floppy", NULL, NULL) == HOST_RETURN_ERROR, "a prefix without * is not an option");
    require(strcmp(text, "Unknown option: floppy\n") == 0, "unknown option message");

    require(run_args("get", "CPU_SPEED", NULL, NULL) == HOST_RETURN_ERROR, "option names are case-sensitive in Amiberry");
    require(strstr(text, "Unknown option: CPU_SPEED") != NULL, "case mismatch is reported as unknown");

    require(run_args("get", "cpu speed", NULL, NULL) == HOST_RETURN_ERROR, "space in option name");
    require(strstr(text, "Option name has a character") != NULL, "bad name is explained");
    require(run_args("get", "a=b", NULL, NULL) == HOST_RETURN_ERROR, "equals in option name");
}

static void test_get_wildcard(void)
{
    fake_reset();
    require(run_args("get", "floppy*", NULL, NULL) == 0, "wildcard get succeeds");
    require(strcmp(text,
                   "floppy_speed=100\n"
                   "floppy0=\nfloppy0type=0\n"
                   "floppy1=\nfloppy1type=0\n"
                   "floppy2=\nfloppy2type=-1\n"
                   "floppy3=\nfloppy3type=-1\n") == 0,
            "wildcard get prints option=value for every match, in order");

    require(run_args("get", "kick*", NULL, NULL) == 0, "single match");
    require(strcmp(text, "kickstart_rom_file=/roms/kick.rom\n") == 0, "single wildcard match");

    require(run_args("get", "nosuch*", NULL, NULL) == HOST_RETURN_ERROR, "no match");
    require(strcmp(text, "No option starts with nosuch\n") == 0, "no match message");

    require(run_args("get", "*", NULL, NULL) == 0, "lone wildcard");
    {
        int lines = 0;

        for (const char *p = text; *p; p++) {
            lines += *p == '\n';
        }
        require(lines == 12, "lone wildcard lists every option");
    }
}

static void test_get_after_failed_search_state(void)
{
    /* A later single get must not inherit the wildcard search. */
    fake_reset();
    require(run_args("get", "floppy*", NULL, NULL) == 0, "wildcard first");
    require(run_args("get", "chipset", NULL, NULL) == 0, "single get after a wildcard");
    require(strcmp(text, "aga\n") == 0, "single get after a wildcard returns the right value");
}

static void test_set(void)
{
    fake_reset();
    require(run_args("set", "cpu_speed", "max", NULL) == 0, "set succeeds");
    require(strcmp(text, "cpu_speed set to max\n") == 0, "set reports what it sent");
    require(fake_set_count == 1 && strcmp(fake_set_lines[0], "cpu_speed=max") == 0, "Amiberry received option and value");

    require(run_args("get", "cpu_speed", NULL, NULL) == 0, "get right after set");
    require(strcmp(text, "real\n") == 0, "the running value changes only when Amiberry applies it");
    fake_run_frames(1);
    require(run_args("get", "cpu_speed", NULL, NULL) == 0, "get after a frame");
    require(strcmp(text, "max\n") == 0, "the new value is visible once applied");
}

static void test_set_joins_value(void)
{
    fake_reset();
    require(run_args("set", "kickstart_rom_file", "/roms/my", "kick.rom") == 0, "multi-word value");
    require(fake_set_count == 1 && strcmp(fake_set_lines[0], "kickstart_rom_file=/roms/my kick.rom") == 0,
            "the rest of the line is one value, quoted for Amiberry's parser");
}

static void test_set_translates_paths(void)
{
    fake_reset();
    require(run_args("set", "kickstart_rom_file", "Work:disks/with space.adf", NULL) == 0, "existing Amiga path");
    require(strcmp(fake_set_lines[0], "kickstart_rom_file=/host/disks/with space.adf") == 0,
            "an existing Amiga path is passed as its host path");
    require(strstr(text, "set to /host/disks/with space.adf") != NULL, "the message shows the host path");

    fake_reset();
    require(run_args("set", "kickstart_rom_file", "/roms/other.rom", NULL) == 0, "host path");
    require(strcmp(fake_set_lines[0], "kickstart_rom_file=/roms/other.rom") == 0,
            "a path that is not an Amiga path goes through unchanged");

    fake_reset();
    require(run_args("set", "kickstart_rom_file", "DH9:hardfile.adf", NULL) == HOST_RETURN_ERROR, "unmapped Amiga path");
    require(strstr(text, "native path translation failed") != NULL, "unmapped path is explained");
    require(fake_set_count == 0, "nothing is sent for an unmapped path");

    fake_reset();
    require(run_args("set", "cpu_speed", "max", NULL) == 0, "keyword while a file of that name exists");
    require(strcmp(fake_set_lines[0], "cpu_speed=max") == 0, "a keyword is never taken for a file in the current directory");
}

static void test_set_errors(void)
{
    char long_value[1100];

    fake_reset();
    require(run_args("set", "no_such_option", "1", NULL) == HOST_RETURN_ERROR, "unknown option");
    require(strstr(text, "Amiberry did not accept no_such_option") != NULL, "unknown option is reported");

    require(run_args("set", "shellexec", "calc", NULL) == HOST_RETURN_ERROR, "shellexec refused");
    require(strstr(text, "host-run") != NULL, "shellexec points to host-run");
    require(run_args("set", "dbg", "x", NULL) == HOST_RETURN_ERROR, "dbg refused");
    require(fake_set_count == 0, "refused options reach nothing");

    require(run_args("set", "cpu_speed", "", NULL) == HOST_RETURN_ERROR, "empty value");
    require(strstr(text, "empty value") != NULL, "empty value is explained");
    require(run_args("set", "cpu_speed", "a\"b", NULL) == HOST_RETURN_ERROR, "quote in value");
    require(run_args("set", "cpu*", "max", NULL) == HOST_RETURN_ERROR, "wildcard in set");
    require(fake_set_count == 0, "bad input reaches nothing");

    memset(long_value, 'x', sizeof(long_value));
    long_value[sizeof(long_value) - 1] = '\0';
    require(run_args("set", "cpu_speed", long_value, NULL) == HOST_RETURN_ERROR, "overlong value");
    require(strstr(text, "too long") != NULL, "overlong value is explained");
    require(fake_set_count == 0, "an overlong value reaches nothing");
}

static void test_quoted_value_survives_amiberry_parser(void)
{
    /* The fake runs Amiberry's own tokenizer: a quoted value stays one token, a bare one splits. */
    char *argv[FAKE_ARGV_MAX + 1];
    int count = fake_cmdlineparser("floppy0 \"/a b/c d.adf\"", argv, FAKE_ARGV_MAX);

    require(count == 2 && strcmp(argv[1], "/a b/c d.adf") == 0, "quoted value is one token");
    free(argv[0]);
    free(argv[1]);
    count = fake_cmdlineparser("floppy0 /a b/c d.adf", argv, FAKE_ARGV_MAX);
    require(count == 4, "an unquoted value would split");
    for (int i = 0; i < count; i++) {
        free(argv[i]);
    }
    count = fake_cmdlineparser("floppy0 \"\"", argv, FAKE_ARGV_MAX);
    require(count == 1, "an empty quoted value vanishes, which is why empty values are refused");
    free(argv[0]);
}

int main(void)
{
    test_no_resource();
    test_usage();
    test_get();
    test_get_wildcard();
    test_get_after_failed_search_state();
    test_set();
    test_set_joins_value();
    test_set_translates_paths();
    test_set_errors();
    test_quoted_value_survives_amiberry_parser();
    return 0;
}
