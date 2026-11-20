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
#include "host_powershell.h"

static inline int host_download_url_supported(const char *url)
{
    return url != NULL &&
           (host_has_prefix_ci(url, "http://") ||
            host_has_prefix_ci(url, "https://") ||
            host_has_prefix_ci(url, "ftp://") ||
            host_has_prefix_ci(url, "ftps://"));
}

#define HOST_DOWNLOAD_PROTOCOLS "=http,https,ftp,ftps"

/*
 * curl options shared by every download. --globoff stops curl from
 * expanding {a,b} and [1-2] in the URL into several transfers.
 */
#define HOST_DOWNLOAD_CURL \
    "curl --globoff --proto '" HOST_DOWNLOAD_PROTOCOLS "'" \
    " --proto-redir '" HOST_DOWNLOAD_PROTOCOLS "'"

/*
 * Reads the headers of a HEAD request and prints exactly one line, the
 * Content-Length of the final response, or 0 when the request failed,
 * the final status is not 2xx, or the server sent no length. The
 * server may refuse HEAD and still serve GET, so a failure here never
 * stops the download.
 */
#define HOST_DOWNLOAD_SIZE_AWK \
    "BEGIN{ok=1} /^HTTP\\//{ok=($2>=200&&$2<300);n=0}" \
    " tolower($1)==\"content-length:\"{n=$2+0} END{printf \"%.0f\\n\",ok?n:0}"

/*
 * Live download for binary-safe pipe sessions: the first line reports
 * the expected size (<= 0 when unknown), then the body streams raw as
 * it downloads. The session exit code is curl's or wget's.
 */
static inline int host_append_download_stream_command(char *command, size_t command_size,
                                                      const char *url)
{
    return host_append_literal(command, command_size,
                              "if command -v curl >/dev/null 2>&1; then "
                              HOST_DOWNLOAD_CURL " --max-time 15 -sIL ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               " 2>/dev/null | tr -d '\\r' | awk '" HOST_DOWNLOAD_SIZE_AWK "'; "
                               HOST_DOWNLOAD_CURL " -sfL ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               "; elif command -v wget >/dev/null 2>&1; then echo 0; wget -q -O - ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size, "; else exit 127; fi");
}

/*
 * The Windows command line expands %NAME% even inside double quotes, so
 * the URL never goes through cmd as data: it travels inside an encoded
 * PowerShell script, which runs curl.exe (shipped with Windows 10 and
 * later) with an argument list. Quotes and control characters cannot be
 * passed to a native program that way.
 */
static inline int host_download_url_windows_safe(const char *url)
{
    if (url == NULL) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)url; *p; p++) {
        if (*p == '"' || *p < 0x20 || *p == 0x7f) {
            return 0;
        }
    }
    return 1;
}

/*
 * Same protocol as the POSIX command. curl's own output is not captured
 * by PowerShell, so the body reaches the pipe unchanged; the session
 * exit code is curl's.
 */
static inline int host_append_download_stream_command_windows(char *command, size_t command_size,
                                                              const char *url)
{
    static char script[HOST_MAX_COMMAND_LEN];

    if (!host_download_url_windows_safe(url)) {
        return 0;
    }

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "$u=") &&
           host_append_ps_quoted(script, sizeof(script), url) &&
           host_append_literal(script, sizeof(script),
                               ";$p='" HOST_DOWNLOAD_PROTOCOLS "';$n=0;"
                               "try{$ok=$true;"
                               "foreach($l in (& curl.exe --globoff --proto $p --proto-redir $p --max-time 15 -sIL $u)){"
                               "if($l -match '^HTTP/\\S+\\s+(\\d+)'){$c=[int]$Matches[1];$ok=($c -ge 200 -and $c -lt 300);$n=0}"
                               "elseif($l -match '^content-length:\\s*(\\d+)'){$n=[int64]$Matches[1]}};"
                               "if(-not $ok){$n=0}}catch{$n=0};"
                               "[Console]::Out.Write(\"$n`n\");"
                               "& curl.exe --globoff --proto $p --proto-redir $p -sfL $u;"
                               "exit $LASTEXITCODE") &&
           host_append_ps_encoded_command(command, command_size, script);
}

/*
 * Two-phase download for pty sessions on older Amiberry builds: the
 * temp file keeps the download's exit code out of the encode pipeline
 * and provides an exact byte count, then the body streams base64
 * encoded (safe through terminal output processing). The download runs
 * in the background while the shell prints a space every second;
 * the guest ignores spaces before the size line, but they show that the
 * host is still working, so a slow download is not mistaken for a
 * stalled one. The curl and wget time limits end a transfer that stops
 * making progress.
 */
static inline int host_append_download_b64_command(char *command, size_t command_size,
                                                   const char *url)
{
    return host_append_literal(command, command_size,
                              "t=$(mktemp) || exit 1; p=;"
                              " trap 'kill \"$p\" 2>/dev/null; rm -f \"$t\"' 0; trap 'exit 1' 1 2 15;"
                              " if command -v curl >/dev/null 2>&1; then "
                              HOST_DOWNLOAD_CURL " --connect-timeout 30 --speed-limit 1 --speed-time 60"
                              " -sfL -o \"$t\" ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               " & p=$!; elif command -v wget >/dev/null 2>&1; then wget -q -T 60 -O \"$t\" ") &&
           host_append_shell_arg(command, command_size, url, 0) &&
           host_append_literal(command, command_size,
                               " & p=$!; else exit 127; fi;"
                               " while kill -0 \"$p\" 2>/dev/null; do printf ' '; sleep 1; done;"
                               " wait \"$p\" || exit 22;"
                               " wc -c < \"$t\"; base64 < \"$t\"");
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

#define HOST_URL_FILENAME_OK 0
#define HOST_URL_FILENAME_TRUNCATED 1
#define HOST_URL_FILENAME_INVALID 2

/*
 * Derive a destination filename from a URL: the last path segment
 * with any query or fragment stripped, or "download" when the URL
 * has no usable segment.
 *
 * The name must be a single AmigaDOS file name, because it is joined to
 * a destination with AddPart(), where "OUT:x" would replace the whole
 * directory. A segment holding ':' (or a control character, or being
 * "." or "..") yields HOST_URL_FILENAME_INVALID and an empty name; the
 * caller should ask for an explicit destination. A name that does not
 * fit in name_size is cut and reported as HOST_URL_FILENAME_TRUNCATED.
 */
static inline int host_url_filename(const char *url, char *name, size_t name_size)
{
    const char *start = url;
    const char *scheme = strstr(url, "://");
    const char *segment = NULL;
    const char *end;
    size_t len;
    int status = HOST_URL_FILENAME_OK;

    if (name_size == 0) {
        return HOST_URL_FILENAME_INVALID;
    }

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

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)segment[i];

        if (c == ':' || c == '/' || c < 0x20 || c == 0x7f) {
            name[0] = '\0';
            return HOST_URL_FILENAME_INVALID;
        }
    }
    if ((len == 1 && segment[0] == '.') || (len == 2 && segment[0] == '.' && segment[1] == '.')) {
        name[0] = '\0';
        return HOST_URL_FILENAME_INVALID;
    }

    if (len >= name_size) {
        len = name_size - 1;
        status = HOST_URL_FILENAME_TRUNCATED;
    }
    memcpy(name, segment, len);
    name[len] = '\0';
    return status;
}

#endif
