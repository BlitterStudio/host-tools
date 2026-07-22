/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_DOWNLOAD_COMMAND_H
#define HOST_DOWNLOAD_COMMAND_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "host_common.h"

static inline int host_download_url_supported(const char *url)
{
    return url != NULL &&
           (host_has_prefix_ci(url, "http://") ||
            host_has_prefix_ci(url, "https://") ||
            host_has_prefix_ci(url, "ftp://") ||
            host_has_prefix_ci(url, "ftps://"));
}

/*
 * Live download for binary-safe pipe sessions: the first line reports
 * the expected size (<= 0 when unknown), then the body streams raw as
 * it downloads. The session exit code is curl's or wget's.
 */
static inline int host_append_download_stream_command(char *command, size_t command_size,
                                                      const char *url)
{
    return host_append_literal(command, command_size,
                              "if command -v curl >/dev/null 2>&1; then"
                              " curl --proto '=http,https,ftp,ftps' --proto-redir '=http,https,ftp,ftps'"
                              " -sfIL -o /dev/null -w '%{content_length}\\n' ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               " 2>/dev/null || echo 0; curl --proto '=http,https,ftp,ftps'"
                               " --proto-redir '=http,https,ftp,ftps' -sfL ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               "; elif command -v wget >/dev/null 2>&1; then echo 0; wget -q -O - ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size, "; else exit 127; fi");
}

/*
 * Live download via the Windows command interpreter: curl.exe ships
 * with Windows 10 and later. Same protocol as the POSIX command; the
 * exit code of "a & b" is b's.
 */
static inline int host_append_download_stream_command_windows(char *command, size_t command_size,
                                                              const char *url)
{
    return host_append_literal(command, command_size,
                              "(curl --proto \"=http,https,ftp,ftps\""
                              " --proto-redir \"=http,https,ftp,ftps\""
                              " -sIL -o NUL -w \"%{content_length}\\n\" ") &&
           host_append_cmd_arg(command, command_size, url) &&
           host_append_literal(command, command_size,
                               " || echo 0) & curl --proto \"=http,https,ftp,ftps\""
                               " --proto-redir \"=http,https,ftp,ftps\" -sfL ") &&
           host_append_cmd_arg(command, command_size, url);
}

/*
 * Two-phase download for pty sessions on older Amiberry builds: the
 * temp file keeps the download's exit code out of the encode pipeline
 * and provides an exact byte count, then the body streams base64
 * encoded (safe through terminal output processing).
 */
static inline int host_append_download_b64_command(char *command, size_t command_size,
                                                   const char *url)
{
    return host_append_literal(command, command_size,
                              "t=$(mktemp) || exit 1;"
                              " if command -v curl >/dev/null 2>&1; then"
                              " curl --proto '=http,https,ftp,ftps'"
                              " --proto-redir '=http,https,ftp,ftps' -sfL -o \"$t\" ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               "; elif command -v wget >/dev/null 2>&1; then wget -q -O \"$t\" ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               "; else rm -f \"$t\"; exit 127; fi"
                               " || { rm -f \"$t\"; exit 22; };"
                               " wc -c < \"$t\"; base64 < \"$t\"; rm -f \"$t\"");
}

/* Keep the sibling component short enough for classic 30-character FFS names. */
static inline int host_download_sidecar_path(const char *destination, char kind,
                                             unsigned long token, unsigned int attempt,
                                             char *path, size_t path_size)
{
    const char *filename;
    char sidecar[16];
    size_t prefix_len;
    int sidecar_len;

    if (destination == NULL || path == NULL || path_size == 0 ||
        (kind != 'p' && kind != 'b') || attempt >= 100) {
        return 0;
    }

    filename = destination;
    for (const char *p = destination; *p; p++) {
        if (*p == '/' || *p == ':') {
            filename = p + 1;
        }
    }
    prefix_len = (size_t)(filename - destination);

    sidecar_len = snprintf(sidecar, sizeof(sidecar), ".ht%c%06lx%02u",
                           kind, token & 0xffffffUL, attempt);
    if (sidecar_len < 0 || (size_t)sidecar_len >= sizeof(sidecar) ||
        prefix_len + (size_t)sidecar_len >= path_size) {
        return 0;
    }

    memcpy(path, destination, prefix_len);
    memcpy(path + prefix_len, sidecar, (size_t)sidecar_len + 1);
    return 1;
}

/*
 * Derive a destination filename from a URL: the last path segment
 * with any query or fragment stripped, or "download" when the URL
 * has no usable segment.
 */
static inline void host_url_filename(const char *url, char *name, size_t name_size)
{
    const char *start = url;
    const char *scheme = strstr(url, "://");
    const char *segment = NULL;
    const char *end;
    size_t len;

    if (scheme != NULL) {
        start = scheme + 3;
    }

    end = start;
    while (*end && *end != '?' && *end != '#') {
        end++;
    }

    for (const char *p = start; p < end; p++) {
        if (*p == '/') {
            segment = p + 1;
        }
    }

    if (segment == NULL || segment == end) {
        segment = "download";
        len = strlen(segment);
    } else {
        len = (size_t)(end - segment);
    }

    if (len >= name_size) {
        len = name_size - 1;
    }
    memcpy(name, segment, len);
    name[len] = '\0';
}

#endif
