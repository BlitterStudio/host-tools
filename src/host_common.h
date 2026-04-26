/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_COMMON_H
#define HOST_COMMON_H

#include <stddef.h>
#include <string.h>

#define HOST_MAX_COMMAND_LEN 4096
#define HOST_MAX_PATH_LEN 4096
#define HOST_RETURN_ERROR 10

static inline int host_filled_buffer(const char *value, size_t max_len)
{
    return max_len > 0 && strlen(value) >= max_len - 1;
}

static inline int host_append_literal(char *dest, size_t max_len, const char *src)
{
    size_t current_len;
    size_t src_len;

    if (max_len == 0 || dest == NULL || src == NULL) {
        return 0;
    }

    current_len = strlen(dest);
    src_len = strlen(src);
    if (current_len >= max_len || src_len > max_len - current_len - 1) {
        return 0;
    }

    memcpy(dest + current_len, src, src_len + 1);
    return 1;
}

static inline int host_join_args(char *dest, size_t max_len, int argc, char *argv[], int start)
{
    for (int i = start; i < argc; i++) {
        if (!host_append_literal(dest, max_len, i > start ? " " : "") ||
            !host_append_literal(dest, max_len, argv[i])) {
            return 0;
        }
    }
    return 1;
}

static inline int host_is_safe_shell_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' ||
           c == '-' || c == '/' || c == ':' || c == '+' ||
           c == '=' || c == ',' || c == '@';
}

static inline size_t host_shell_arg_len(const char *src, int *needs_quote)
{
    size_t len = 0;
    int quote = (src[0] == '\0');

    for (const char *p = src; *p; p++) {
        if (!host_is_safe_shell_char(*p)) {
            quote = 1;
        }
    }

    if (!quote) {
        *needs_quote = 0;
        return strlen(src);
    }

    for (const char *p = src; *p; p++) {
        len += (*p == '\'') ? 4 : 1;
    }

    *needs_quote = 1;
    return len + 2;
}

static inline int host_append_shell_arg(char *dest, size_t max_len, const char *src, int add_separator)
{
    size_t current_len;
    size_t available;
    size_t arg_len;
    size_t pos;
    int needs_quote;

    if (max_len == 0 || dest == NULL || src == NULL) {
        return 0;
    }

    current_len = strlen(dest);
    if (current_len >= max_len) {
        return 0;
    }

    available = max_len - current_len - 1;
    if (add_separator) {
        if (available == 0) {
            return 0;
        }
        available--;
    }

    arg_len = host_shell_arg_len(src, &needs_quote);
    if (arg_len > available) {
        return 0;
    }

    pos = current_len;
    if (add_separator) {
        dest[pos++] = ' ';
    }

    if (!needs_quote) {
        while (*src) {
            dest[pos++] = *src++;
        }
    } else {
        dest[pos++] = '\'';
        for (const char *p = src; *p; p++) {
            if (*p == '\'') {
                dest[pos++] = '\'';
                dest[pos++] = '\\';
                dest[pos++] = '\'';
                dest[pos++] = '\'';
            } else {
                dest[pos++] = *p;
            }
        }
        dest[pos++] = '\'';
    }

    dest[pos] = '\0';
    return 1;
}

static inline int host_append_shell_literal_and_arg(char *dest, size_t max_len,
                                                   const char *literal, const char *arg)
{
    return host_append_literal(dest, max_len, literal) &&
           host_append_shell_arg(dest, max_len, arg, 0);
}

static inline char host_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static inline int host_has_prefix_ci(const char *value, const char *prefix)
{
    while (*prefix) {
        if (host_ascii_lower(*value) != host_ascii_lower(*prefix)) {
            return 0;
        }
        value++;
        prefix++;
    }
    return 1;
}

static inline int host_is_uri(const char *value)
{
    static const char *schemes[] = {
        "data:", "file:", "ftp:", "geo:", "http:", "https:",
        "irc:", "ircs:", "magnet:", "mailto:", "news:", "nntp:",
        "sip:", "sips:", "sms:", "tel:", "urn:", "webcal:",
        "xmpp:", NULL
    };

    if (strstr(value, "://") != NULL) {
        return 1;
    }

    for (int i = 0; schemes[i] != NULL; i++) {
        if (host_has_prefix_ci(value, schemes[i])) {
            return 1;
        }
    }

    return 0;
}

#endif
