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

/*
 * Quote an argument for the Windows command interpreter. Embedded
 * quotes and line breaks are rejected rather than escaped.
 */
static inline int host_append_cmd_arg(char *command, size_t command_size, const char *arg)
{
    for (const char *p = arg; *p; p++) {
        if (*p == '"' || *p == '\n' || *p == '\r') {
            return 0;
        }
    }
    return host_append_literal(command, command_size, "\"") &&
           host_append_literal(command, command_size, arg) &&
           host_append_literal(command, command_size, "\"");
}

static inline int host_put_char(char *dest, size_t max_len, size_t *pos, char c)
{
    if (*pos + 1 >= max_len) {
        return 0;
    }
    dest[(*pos)++] = c;
    return 1;
}

static inline int host_put_repeat(char *dest, size_t max_len, size_t *pos, char c, size_t count)
{
    while (count-- > 0) {
        if (!host_put_char(dest, max_len, pos, c)) {
            return 0;
        }
    }
    return 1;
}

/*
 * Append one argument to a Windows command line using the quoting that
 * CommandLineToArgvW and the Microsoft C runtime undo: the argument is
 * quoted when it is empty or holds a space, tab or quote; a quote
 * becomes \" and backslashes in front of a quote, or in front of the
 * closing quote, are doubled. dest is left unchanged on failure.
 */
static inline int host_append_cmdline_arg(char *dest, size_t max_len, const char *src,
                                          int add_separator)
{
    size_t start;
    size_t pos;
    size_t backslashes = 0;
    int quote;
    int ok = 1;

    if (max_len == 0 || dest == NULL || src == NULL) {
        return 0;
    }

    quote = (src[0] == '\0');
    start = strlen(dest);
    if (start >= max_len) {
        return 0;
    }
    pos = start;

    for (const char *p = src; *p; p++) {
        if (*p == ' ' || *p == '\t' || *p == '"') {
            quote = 1;
        }
    }

    if (add_separator) {
        ok = host_put_char(dest, max_len, &pos, ' ');
    }
    if (ok && quote) {
        ok = host_put_char(dest, max_len, &pos, '"');
    }
    for (const char *p = src; ok && *p; p++) {
        if (*p == '\\') {
            backslashes++;
            continue;
        }
        if (*p == '"') {
            ok = host_put_repeat(dest, max_len, &pos, '\\', backslashes * 2 + 1) &&
                 host_put_char(dest, max_len, &pos, '"');
        } else {
            ok = host_put_repeat(dest, max_len, &pos, '\\', backslashes) &&
                 host_put_char(dest, max_len, &pos, *p);
        }
        backslashes = 0;
    }
    if (ok) {
        ok = host_put_repeat(dest, max_len, &pos, '\\', quote ? backslashes * 2 : backslashes) &&
             (!quote || host_put_char(dest, max_len, &pos, '"'));
    }

    if (!ok) {
        dest[start] = '\0';
        return 0;
    }
    dest[pos] = '\0';
    return 1;
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

#define HOST_FILE_URI_MAX_LEN (HOST_MAX_PATH_LEN * 3 + 8)

static inline int host_path_to_file_uri(const char *path, char *uri, size_t uri_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;

    if (path == NULL || uri == NULL || uri_size < 8 || path[0] != '/') {
        return 0;
    }

    memcpy(uri, "file://", 7);
    pos = 7;

    for (const unsigned char *p = (const unsigned char *)path; *p; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
            (*p >= '0' && *p <= '9') || *p == '/' || *p == '-' ||
            *p == '.' || *p == '_' || *p == '~') {
            if (pos + 1 >= uri_size) {
                return 0;
            }
            uri[pos++] = (char)*p;
        } else {
            if (pos + 3 >= uri_size) {
                return 0;
            }
            uri[pos++] = '%';
            uri[pos++] = hex[*p >> 4];
            uri[pos++] = hex[*p & 0x0f];
        }
    }

    uri[pos] = '\0';
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
