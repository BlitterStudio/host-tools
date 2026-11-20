/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <string.h>
#include "host_config_access.h"
#include "host_path.h"

static const char version[] = "$VER: Host-Config " VERSION_STR " (" DATE_STR ")";

static int print_usage(void)
{
    printf("Host-Config v%s\n", VERSION_STR);
    printf("Host-Config reads and changes Amiberry configuration options.\n");
    printf("%s\nUsage: host-config get <option>\n"
           "       host-config get <prefix>*\n"
           "       host-config set <option> <value...>\n", version);
    return 0;
}

static int check_option(const char *option, int allow_wildcard)
{
    int status = host_config_check_option(option, allow_wildcard);

    if (status != HOST_CONFIG_ARG_OK) {
        printf("Option name %s\n", host_config_arg_error(status));
        return 0;
    }
    return 1;
}

static int list_matches(const char *option)
{
    static char reply[HOST_CONFIG_REPLY_MAX];
    long rc;
    int status;

    status = host_config_begin_search(option, &rc);
    for (int index = 0; status == HOST_CONFIG_FOUND; index++) {
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            SetSignal(0, SIGBREAKF_CTRL_C);
            printf("***Break\n");
            return HOST_RETURN_ERROR;
        }
        status = host_config_next_match(index, reply, sizeof(reply), &rc);
        if (status == HOST_CONFIG_FOUND) {
            printf("%s\n", reply);
        } else if (status == HOST_CONFIG_END) {
            return 0;
        }
    }

    if (status == HOST_CONFIG_MISSING) {
        printf("No option starts with %.*s\n", (int)(strlen(option) - 1), option);
    } else {
        printf("Amiberry could not read its configuration (code %ld)\n", rc);
    }
    return HOST_RETURN_ERROR;
}

static int get_option(const char *option)
{
    static char reply[HOST_CONFIG_REPLY_MAX];
    long rc;
    int status;

    if (host_config_option_is_wildcard(option)) {
        return list_matches(option);
    }

    status = host_config_get(option, reply, sizeof(reply), &rc);
    if (status == HOST_CONFIG_FOUND) {
        printf("%s\n", reply);
        return 0;
    }
    if (status == HOST_CONFIG_MISSING) {
        printf("Unknown option: %s\n", option);
    } else {
        printf("Amiberry could not read its configuration (code %ld)\n", rc);
    }
    return HOST_RETURN_ERROR;
}

static int set_option(const char *option, int argc, char *argv[], int first_value)
{
    static char value[HOST_CONFIG_VALUE_MAX + 1];
    static char host_path[HOST_MAX_PATH_LEN];
    static char line[HOST_CONFIG_LINE_MAX];
    static char reply[HOST_CONFIG_REPLY_MAX];
    const char *target;
    int status;
    long rc;

    if (host_config_option_is_blocked(option)) {
        printf("%s is not available through host-config. Use host-run to run a host command.\n", option);
        return HOST_RETURN_ERROR;
    }

    value[0] = '\0';
    if (!host_join_args(value, sizeof(value), argc, argv, first_value)) {
        printf("Value is too long\n");
        return HOST_RETURN_ERROR;
    }
    status = host_config_check_value(value);
    if (status == HOST_CONFIG_ARG_EMPTY) {
        printf("Amiberry cannot set an empty value\n");
        return HOST_RETURN_ERROR;
    }
    if (status != HOST_CONFIG_ARG_OK) {
        printf("Value %s\n", host_config_arg_error(status));
        return HOST_RETURN_ERROR;
    }

    target = value;
    if (host_config_value_may_be_path(value)) {
        int path_status = host_resolve_optional_path(value, host_path, sizeof(host_path), &target);

        if (path_status != HOST_PATH_OK) {
            printf("%s: %s\n", host_path_error(path_status), value);
            return HOST_RETURN_ERROR;
        }
    }

    status = host_config_build_set_line(line, sizeof(line), option, target);
    if (status != HOST_CONFIG_ARG_OK) {
        printf("Value %s\n", host_config_arg_error(status));
        return HOST_RETURN_ERROR;
    }

    /* Amiberry accepts an unknown option with result 0 and ignores it, so an
     * option that the configuration does not list is rejected here. */
    status = host_config_get(option, reply, sizeof(reply), &rc);
    if (status == HOST_CONFIG_MISSING) {
        printf("Unknown option: %s\n", option);
        return HOST_RETURN_ERROR;
    }
    if (status != HOST_CONFIG_FOUND) {
        printf("Amiberry could not read its configuration (code %ld)\n", rc);
        return HOST_RETURN_ERROR;
    }

    rc = host_config_apply(line);
    if (rc == HOST_CONFIG_SET_REJECTED) {
        printf("Amiberry did not accept %s: invalid value\n", option);
        return HOST_RETURN_ERROR;
    }
    if (rc != 0) {
        printf("Amiberry could not set %s (code %ld)\n", option, rc);
        return HOST_RETURN_ERROR;
    }

    printf("%s set to %s\n", option, target);
    return 0;
}

int main(int argc, char *argv[])
{
    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing config command\n");
        print_usage();
        return HOST_RETURN_ERROR;
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    if (strcmp(argv[1], "get") == 0) {
        if (argc != 3) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!check_option(argv[2], 1)) {
            return HOST_RETURN_ERROR;
        }
        return get_option(argv[2]);
    }

    if (strcmp(argv[1], "set") == 0) {
        if (argc < 4) {
            printf("Usage error\n");
            print_usage();
            return HOST_RETURN_ERROR;
        }
        if (!check_option(argv[2], 0)) {
            return HOST_RETURN_ERROR;
        }
        return set_option(argv[2], argc, argv, 3);
    }

    printf("Unknown config command\n");
    print_usage();
    return HOST_RETURN_ERROR;
}
