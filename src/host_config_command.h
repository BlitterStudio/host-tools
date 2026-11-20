/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CONFIG_COMMAND_H
#define HOST_CONFIG_COMMAND_H

#include <stddef.h>
#include <string.h>

/*
 * Amiberry parses the text given to EmuConfigModify (trap 82) into
 * space-separated tokens, with double quotes grouping a token. It has no
 * escape for a quote and drops control characters, and an empty token is
 * lost, so these helpers refuse what the parser cannot carry.
 *
 * The parser also copies each token into a buffer of MAX_DPATH characters
 * without checking the length. MAX_DPATH is 260 on Windows hosts, the
 * smallest on any host, so a token of 260 characters or more overruns the
 * buffer. Options and values stop at 259 characters.
 */

#define HOST_CONFIG_OPTION_MAX 128
#define HOST_CONFIG_VALUE_MAX 259
#define HOST_CONFIG_LINE_MAX (HOST_CONFIG_OPTION_MAX + HOST_CONFIG_VALUE_MAX + 8)
/* Amiberry's longest saved configuration line is 2560 characters. */
#define HOST_CONFIG_REPLY_MAX 2600

#define HOST_CONFIG_ARG_OK 0
#define HOST_CONFIG_ARG_EMPTY 1
#define HOST_CONFIG_ARG_TOO_LONG 2
#define HOST_CONFIG_ARG_BAD_CHAR 3
#define HOST_CONFIG_ARG_BLOCKED 4

static inline int host_config_ascii_lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

static inline int host_config_name_equals(const char *a, const char *b)
{
    while (*a && *b) {
        if (host_config_ascii_lower((unsigned char)*a) != host_config_ascii_lower((unsigned char)*b)) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

/*
 * Option names are printable ASCII without space, quote or equals sign.
 * A trailing '*' is a prefix search and is allowed only when requested.
 */
static inline int host_config_check_option(const char *name, int allow_wildcard)
{
    size_t len = strlen(name);

    if (len == 0) {
        return HOST_CONFIG_ARG_EMPTY;
    }
    if (len > HOST_CONFIG_OPTION_MAX) {
        return HOST_CONFIG_ARG_TOO_LONG;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];

        if (c <= ' ' || c >= 127 || c == '"' || c == '=') {
            return HOST_CONFIG_ARG_BAD_CHAR;
        }
        if (c == '*' && !(allow_wildcard && i + 1 == len)) {
            return HOST_CONFIG_ARG_BAD_CHAR;
        }
    }
    return HOST_CONFIG_ARG_OK;
}

static inline int host_config_option_is_wildcard(const char *name)
{
    size_t len = strlen(name);

    return len > 0 && name[len - 1] == '*';
}

/*
 * These two options run host commands (shellexec) or the debugger (dbg).
 * Running host commands is what Native Code enables for the other tools,
 * and trap 82 would let host-config bypass that switch.
 */
static inline int host_config_option_is_blocked(const char *name)
{
    return host_config_name_equals(name, "shellexec") || host_config_name_equals(name, "dbg");
}

static inline int host_config_check_value(const char *value)
{
    size_t len = strlen(value);

    if (len == 0) {
        return HOST_CONFIG_ARG_EMPTY;
    }
    if (len > HOST_CONFIG_VALUE_MAX) {
        return HOST_CONFIG_ARG_TOO_LONG;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)value[i];

        if (c < ' ' || c == 127 || c == '"') {
            return HOST_CONFIG_ARG_BAD_CHAR;
        }
    }
    return HOST_CONFIG_ARG_OK;
}

/*
 * Builds "<option> <value>", quoting the value when it holds a space.
 * dest must hold HOST_CONFIG_LINE_MAX bytes.
 */
static inline int host_config_build_set_line(char *dest, size_t size, const char *option, const char *value)
{
    int status;
    int quote;
    size_t option_len;
    size_t value_len;
    size_t pos;

    if (size == 0) {
        return HOST_CONFIG_ARG_TOO_LONG;
    }
    dest[0] = '\0';

    status = host_config_check_option(option, 0);
    if (status != HOST_CONFIG_ARG_OK) {
        return status;
    }
    if (host_config_option_is_blocked(option)) {
        return HOST_CONFIG_ARG_BLOCKED;
    }
    status = host_config_check_value(value);
    if (status != HOST_CONFIG_ARG_OK) {
        return status;
    }

    quote = strchr(value, ' ') != NULL;
    option_len = strlen(option);
    value_len = strlen(value);
    if (option_len + 1 + value_len + (quote ? 2 : 0) + 1 > size) {
        return HOST_CONFIG_ARG_TOO_LONG;
    }

    memcpy(dest, option, option_len);
    pos = option_len;
    dest[pos++] = ' ';
    if (quote) {
        dest[pos++] = '"';
    }
    memcpy(dest + pos, value, value_len);
    pos += value_len;
    if (quote) {
        dest[pos++] = '"';
    }
    dest[pos] = '\0';
    return HOST_CONFIG_ARG_OK;
}

/*
 * Only a value that has a colon, slash or dot can be a path worth looking
 * up. This keeps a file named "max" in the current directory from replacing
 * the value "max" in "host-config set cpu_speed max".
 */
static inline int host_config_value_may_be_path(const char *value)
{
    return strchr(value, ':') != NULL || strchr(value, '/') != NULL || strchr(value, '.') != NULL;
}

static inline const char *host_config_arg_error(int status)
{
    switch (status) {
        case HOST_CONFIG_ARG_EMPTY:
            return "is empty";
        case HOST_CONFIG_ARG_TOO_LONG:
            return "is too long";
        case HOST_CONFIG_ARG_BAD_CHAR:
            return "has a character Amiberry cannot accept";
        case HOST_CONFIG_ARG_BLOCKED:
            return "is not available here";
        default:
            return "is invalid";
    }
}

/* Parses an optionally signed decimal integer; trailing text is an error. */
static inline int host_config_parse_int(const char *text, long *result)
{
    long value = 0;
    int negative = 0;

    if (*text == '-') {
        negative = 1;
        text++;
    } else if (*text == '+') {
        text++;
    }
    if (*text == '\0') {
        return 0;
    }
    for (; *text; text++) {
        if (*text < '0' || *text > '9' || value > 100000000L) {
            return 0;
        }
        value = value * 10 + (*text - '0');
    }
    *result = negative ? -value : value;
    return 1;
}

#endif
