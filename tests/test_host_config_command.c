/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_config_command.h"
#include "host_disk_command.h"

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "%s\n", message);
        exit(1);
    }
}

static void test_option_names(void)
{
    char long_name[HOST_CONFIG_OPTION_MAX + 2];

    require(host_config_check_option("cpu_speed", 0) == HOST_CONFIG_ARG_OK, "plain option");
    require(host_config_check_option("input.1.joystick.0.button.0", 0) == HOST_CONFIG_ARG_OK,
            "dots are part of option names");
    require(host_config_check_option("", 0) == HOST_CONFIG_ARG_EMPTY, "empty option");
    require(host_config_check_option("cpu speed", 0) == HOST_CONFIG_ARG_BAD_CHAR, "space in option");
    require(host_config_check_option("cpu_speed=max", 0) == HOST_CONFIG_ARG_BAD_CHAR, "equals in option");
    require(host_config_check_option("cpu\"speed", 0) == HOST_CONFIG_ARG_BAD_CHAR, "quote in option");
    require(host_config_check_option("cpu_sp\xe9" "ed", 0) == HOST_CONFIG_ARG_BAD_CHAR, "non-ASCII in option");
    require(host_config_check_option("floppy*", 0) == HOST_CONFIG_ARG_BAD_CHAR, "wildcard refused for set");
    require(host_config_check_option("floppy*", 1) == HOST_CONFIG_ARG_OK, "trailing wildcard for get");
    require(host_config_check_option("flo*ppy", 1) == HOST_CONFIG_ARG_BAD_CHAR, "inner wildcard refused");
    require(host_config_check_option("*", 1) == HOST_CONFIG_ARG_OK, "lone wildcard lists everything");

    memset(long_name, 'a', sizeof(long_name));
    long_name[HOST_CONFIG_OPTION_MAX] = '\0';
    require(host_config_check_option(long_name, 0) == HOST_CONFIG_ARG_OK, "longest option");
    long_name[HOST_CONFIG_OPTION_MAX] = 'a';
    long_name[HOST_CONFIG_OPTION_MAX + 1] = '\0';
    require(host_config_check_option(long_name, 0) == HOST_CONFIG_ARG_TOO_LONG, "overlong option");

    require(host_config_option_is_wildcard("floppy*"), "wildcard detected");
    require(!host_config_option_is_wildcard("floppy0"), "plain option is not a wildcard");
}

static void test_blocked_options(void)
{
    require(host_config_option_is_blocked("shellexec"), "shellexec is blocked");
    require(host_config_option_is_blocked("ShellExec"), "blocking ignores case like Amiberry's lookup");
    require(host_config_option_is_blocked("dbg"), "dbg is blocked");
    require(!host_config_option_is_blocked("dbgx"), "only the exact names are blocked");
    require(!host_config_option_is_blocked("shellexec2"), "prefix is not blocked");
    require(!host_config_option_is_blocked("cpu_speed"), "ordinary option is not blocked");
}

static void test_values(void)
{
    char value[HOST_CONFIG_VALUE_MAX + 2];

    require(host_config_check_value("max") == HOST_CONFIG_ARG_OK, "plain value");
    require(host_config_check_value("a b c") == HOST_CONFIG_ARG_OK, "spaces are allowed");
    require(host_config_check_value("C:\\Users\\me\\a.adf") == HOST_CONFIG_ARG_OK, "backslashes are allowed");
    require(host_config_check_value("caf\xe9") == HOST_CONFIG_ARG_OK, "ISO-8859-1 bytes are allowed");
    require(host_config_check_value("") == HOST_CONFIG_ARG_EMPTY, "empty value");
    require(host_config_check_value("a\"b") == HOST_CONFIG_ARG_BAD_CHAR, "quote cannot be escaped");
    require(host_config_check_value("a\tb") == HOST_CONFIG_ARG_BAD_CHAR, "control characters are dropped by Amiberry");
    require(host_config_check_value("a\nb") == HOST_CONFIG_ARG_BAD_CHAR, "line break ends Amiberry's input");

    memset(value, 'v', sizeof(value));
    value[HOST_CONFIG_VALUE_MAX] = '\0';
    require(host_config_check_value(value) == HOST_CONFIG_ARG_OK, "longest value");
    value[HOST_CONFIG_VALUE_MAX] = 'v';
    value[HOST_CONFIG_VALUE_MAX + 1] = '\0';
    require(host_config_check_value(value) == HOST_CONFIG_ARG_TOO_LONG, "overlong value");
}

static void test_set_line(void)
{
    char line[HOST_CONFIG_LINE_MAX];
    char small[16];
    char value[HOST_CONFIG_VALUE_MAX + 1];

    require(host_config_build_set_line(line, sizeof(line), "cpu_speed", "max") == HOST_CONFIG_ARG_OK, "plain line");
    require(strcmp(line, "cpu_speed max") == 0, "plain value is not quoted");

    require(host_config_build_set_line(line, sizeof(line), "floppy0", "/home/me/My Disks/a.adf") == HOST_CONFIG_ARG_OK,
            "value with spaces");
    require(strcmp(line, "floppy0 \"/home/me/My Disks/a.adf\"") == 0, "value with spaces is quoted");

    require(host_config_build_set_line(line, sizeof(line), "floppy0", "C:\\Users\\me\\a.adf") == HOST_CONFIG_ARG_OK,
            "Windows path");
    require(strcmp(line, "floppy0 C:\\Users\\me\\a.adf") == 0, "backslashes pass through");

    require(host_config_build_set_line(line, sizeof(line), "floppy0", "C:\\My Disks\\") == HOST_CONFIG_ARG_OK,
            "trailing backslash");
    require(strcmp(line, "floppy0 \"C:\\My Disks\\\"") == 0, "a trailing backslash does not escape the closing quote");

    require(host_config_build_set_line(line, sizeof(line), "shellexec", "calc") == HOST_CONFIG_ARG_BLOCKED,
            "shellexec refused");
    require(line[0] == '\0', "a refused line is empty");
    require(host_config_build_set_line(line, sizeof(line), "cpu_speed", "") == HOST_CONFIG_ARG_EMPTY, "empty value");
    require(host_config_build_set_line(line, sizeof(line), "", "max") == HOST_CONFIG_ARG_EMPTY, "empty option");
    require(host_config_build_set_line(line, sizeof(line), "cpu speed", "max") == HOST_CONFIG_ARG_BAD_CHAR,
            "bad option");
    require(host_config_build_set_line(line, sizeof(line), "cpu_speed", "a\"b") == HOST_CONFIG_ARG_BAD_CHAR,
            "bad value");

    require(host_config_build_set_line(small, sizeof(small), "cpu_speed", "maxim") == HOST_CONFIG_ARG_OK,
            "line that fits exactly");
    require(host_config_build_set_line(small, sizeof(small), "cpu_speed", "maxima") == HOST_CONFIG_ARG_TOO_LONG,
            "line that is one byte too long");
    require(host_config_build_set_line(small, sizeof(small), "chip", "a b") == HOST_CONFIG_ARG_OK,
            "quotes count toward the size");
    require(host_config_build_set_line(small, sizeof(small), "chipset_xx", "a b") == HOST_CONFIG_ARG_TOO_LONG,
            "quotes count toward the size when too long");

    memset(value, 'v', sizeof(value));
    value[HOST_CONFIG_VALUE_MAX] = '\0';
    value[10] = ' ';
    require(host_config_build_set_line(line, sizeof(line), "kickstart_rom_file", value) == HOST_CONFIG_ARG_OK,
            "longest quoted value fits the line buffer");
}

static void test_path_heuristic(void)
{
    require(host_config_value_may_be_path("Work:disks/a.adf"), "volume path");
    require(host_config_value_may_be_path("dir/file"), "relative path with a slash");
    require(host_config_value_may_be_path("a.adf"), "file name with an extension");
    require(!host_config_value_may_be_path("max"), "keyword");
    require(!host_config_value_may_be_path("68040"), "number");
    require(!host_config_value_may_be_path("true"), "boolean");
}

static void test_parse_int(void)
{
    long value = 77;

    require(host_config_parse_int("0", &value) && value == 0, "zero");
    require(host_config_parse_int("-1", &value) && value == -1, "negative");
    require(host_config_parse_int("+3", &value) && value == 3, "explicit plus");
    require(host_config_parse_int("1234", &value) && value == 1234, "several digits");
    value = 77;
    require(!host_config_parse_int("", &value) && value == 77, "empty is not a number");
    require(!host_config_parse_int("-", &value), "lone sign");
    require(!host_config_parse_int("1x", &value), "trailing text");
    require(!host_config_parse_int("max", &value), "word");
    require(!host_config_parse_int("99999999999999999999", &value), "overflow");
}

static void test_drives(void)
{
    char option[24];
    char tiny[8];
    char line[80];

    require(host_disk_parse_drive("0") == 0 && host_disk_parse_drive("3") == 3, "digits");
    require(host_disk_parse_drive("DF0:") == 0, "DF0:");
    require(host_disk_parse_drive("df2:") == 2, "lower case");
    require(host_disk_parse_drive("Df1") == 1, "colon is optional");
    require(host_disk_parse_drive("DF3") == 3, "DF3");
    require(host_disk_parse_drive("4") == -1, "drive 4");
    require(host_disk_parse_drive("DF4:") == -1, "DF4:");
    require(host_disk_parse_drive("-1") == -1, "negative");
    require(host_disk_parse_drive("00") == -1, "two digits");
    require(host_disk_parse_drive("") == -1, "empty");
    require(host_disk_parse_drive("DF") == -1, "no number");
    require(host_disk_parse_drive("DF0:file.adf") == -1, "a file after the drive");
    require(host_disk_parse_drive("DF0::") == -1, "two colons");
    require(host_disk_parse_drive("RAM:") == -1, "another device");

    require(host_disk_type_option(2, option, sizeof(option)) && strcmp(option, "floppy2type") == 0, "type option");
    require(!host_disk_type_option(4, option, sizeof(option)), "type option for drive 4");
    require(!host_disk_type_option(-1, option, sizeof(option)), "type option for drive -1");
    require(!host_disk_type_option(0, tiny, sizeof(tiny)), "type option buffer too small");

    host_disk_format_line(line, sizeof(line), 0, 1, 1, "/host/a.adf");
    require(strcmp(line, "DF0: /host/a.adf") == 0, "line with a disk");
    host_disk_format_line(line, sizeof(line), 1, 1, 0, "");
    require(strcmp(line, "DF1: (empty)") == 0, "line for an empty drive");
    host_disk_format_line(line, sizeof(line), 2, 0, 0, "");
    require(strcmp(line, "DF2: (no drive)") == 0, "line for a missing drive");
    host_disk_format_line(line, sizeof(line), 3, 0, 1, "/host/a.adf");
    require(strcmp(line, "DF3: (no drive)") == 0, "a missing drive hides a stale image");
    host_disk_format_line(line, sizeof(line), 0, 1, 0, "/host/a.adf");
    require(strcmp(line, "DF0: /host/a.adf (not loaded)") == 0, "line for an image that did not load");
}

int main(void)
{
    test_option_names();
    test_blocked_options();
    test_values();
    test_set_line();
    test_path_heuristic();
    test_parse_int();
    test_drives();
    return 0;
}
