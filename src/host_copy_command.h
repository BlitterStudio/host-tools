/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_COPY_COMMAND_H
#define HOST_COPY_COMMAND_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host_common.h"
#include "host_powershell.h"

/* Largest file the tool moves: the Amiga file size is a signed 32-bit value. */
#define HOST_COPY_MAX_SIZE 0x7fffffffUL

/*
 * Guest text is ISO-8859-1. POSIX hosts expect UTF-8 file names, so a
 * path typed on the Amiga is converted before it reaches the shell.
 * PowerShell scripts take ISO-8859-1 directly (see
 * host_base64_encode_utf16le), so the Windows builders skip this step.
 */
static inline int host_copy_latin1_to_utf8(const char *src, char *dest, size_t dest_size)
{
    size_t pos = 0;

    for (const unsigned char *p = (const unsigned char *)src; *p; p++) {
        if (*p < 0x80) {
            if (pos + 1 >= dest_size) {
                return 0;
            }
            dest[pos++] = (char)*p;
        } else {
            if (pos + 2 >= dest_size) {
                return 0;
            }
            dest[pos++] = (char)(0xC0 | (*p >> 6));
            dest[pos++] = (char)(0x80 | (*p & 0x3F));
        }
    }
    if (pos >= dest_size) {
        return 0;
    }
    dest[pos] = '\0';
    return 1;
}

/* The part of a host path after its last separator ("" for a trailing one). */
static inline const char *host_copy_host_basename(const char *path, int windows)
{
    const char *name = path;

    for (const char *p = path; *p; p++) {
        if (*p == '/' || (windows && (*p == '\\' || *p == ':'))) {
            name = p + 1;
        }
    }
    return name;
}

/* A host file name that can be a single AmigaDOS path component. */
static inline int host_copy_amiga_name_ok(const char *name)
{
    if (name[0] == '\0' || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    for (const char *p = name; *p; p++) {
        if (*p == '/' || *p == ':') {
            return 0;
        }
    }
    return 1;
}

static inline int host_copy_windows_reserved_name(const char *name)
{
    static const char *const devices[] = { "con", "prn", "aux", "nul" };
    char stem[8];
    size_t len = 0;

    while (name[len] && name[len] != '.' && len < sizeof(stem) - 1) {
        stem[len] = host_ascii_lower(name[len]);
        len++;
    }
    stem[len] = '\0';
    if (name[len] != '\0' && name[len] != '.') {
        return 0;
    }
    for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
        if (strcmp(stem, devices[i]) == 0) {
            return 1;
        }
    }
    return (strncmp(stem, "com", 3) == 0 || strncmp(stem, "lpt", 3) == 0) &&
           stem[3] >= '1' && stem[3] <= '9' && stem[4] == '\0';
}

/*
 * An Amiga file name that can be a single path component in a host
 * folder. Windows rejects more characters and the device names.
 */
static inline int host_copy_host_name_ok(const char *name, int windows)
{
    size_t len = strlen(name);

    if (len == 0 || strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    for (const char *p = name; *p; p++) {
        unsigned char c = (unsigned char)*p;

        if (c == '/' || (windows && (c < 0x20 || strchr("\\:*?\"<>|", c) != NULL))) {
            return 0;
        }
    }
    if (windows && (name[len - 1] == '.' || name[len - 1] == ' ' ||
                    host_copy_windows_reserved_name(name))) {
        return 0;
    }
    return 1;
}

static inline int host_copy_home_prefix(const char *path)
{
    return path[0] == '~' && (path[1] == '\0' || path[1] == '/');
}

/*
 * A leading "~" or "~/" means the user's home folder; the quoted
 * string would otherwise reach the host as a literal "~".
 */
static inline int host_copy_append_posix_path(char *command, size_t command_size, const char *path)
{
    static char utf8[HOST_MAX_PATH_LEN];

    if (!host_copy_latin1_to_utf8(path, utf8, sizeof(utf8))) {
        return 0;
    }
    if (host_copy_home_prefix(utf8)) {
        return host_append_literal(command, command_size, "\"$HOME\"") &&
               (utf8[1] == '\0' || host_append_shell_arg(command, command_size, utf8 + 1, 0));
    }
    return host_append_shell_arg(command, command_size, utf8, 0);
}

/*
 * The PowerShell expression for a host path; "~" is the user profile
 * folder. The home form is parenthesized so it also works as a command
 * argument.
 */
static inline int host_copy_append_ps_path(char *script, size_t script_size, const char *path)
{
    if (host_copy_home_prefix(path) || (path[0] == '~' && path[1] == '\\')) {
        return host_append_literal(script, script_size,
                                   "([Environment]::GetFolderPath('UserProfile')") &&
               (path[1] == '\0' ||
                (host_append_literal(script, script_size, "+") &&
                 host_append_ps_quoted(script, script_size, path + 1))) &&
               host_append_literal(script, script_size, ")");
    }
    return host_append_ps_quoted(script, script_size, path);
}

static inline int host_copy_append_number(char *command, size_t command_size, unsigned long value)
{
    char digits[16];

    snprintf(digits, sizeof(digits), "%lu", value);
    return host_append_literal(command, command_size, digits);
}

/*
 * get, POSIX host: exit 2 when the path is not a regular file, 3 when it
 * cannot be read. Otherwise the first line is the size in bytes, then
 * the file follows raw; the exit code is cat's.
 */
static inline int host_append_copy_get_command(char *command, size_t command_size, const char *path)
{
    return host_append_literal(command, command_size, "f=") &&
           host_copy_append_posix_path(command, command_size, path) &&
           host_append_literal(command, command_size,
                               "; [ -f \"$f\" ] || exit 2; [ -r \"$f\" ] || exit 3;"
                               " wc -c <\"$f\"; cat -- \"$f\"");
}

/* get, Windows host: same protocol as the POSIX command, through PowerShell. */
static inline int host_append_copy_get_command_windows(char *command, size_t command_size,
                                                       const char *path)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "try{$f=Get-Item -Force -LiteralPath ") &&
           host_copy_append_ps_path(script, sizeof(script), path) &&
           host_append_literal(script, sizeof(script),
                               "}catch{exit 2};if($f -isnot [IO.FileInfo]){exit 2};"
                               "try{$s=[IO.File]::Open($f.FullName,'Open','Read','ReadWrite')}catch{exit 3};"
                               "$o=[Console]::OpenStandardOutput();"
                               "$h=[Text.Encoding]::ASCII.GetBytes(\"$($s.Length)`n\");"
                               "$o.Write($h,0,$h.Length);$s.CopyTo($o);$s.Dispose();$o.Flush()") &&
           host_append_ps_encoded_command(command, command_size, script);
}

/*
 * put, POSIX host. The host checks the target and creates a temporary
 * file next to it, answers "READY", then reads exactly len bytes from
 * stdin (the pipe session cannot close only stdin) and moves the file
 * into place. The last line is "OK <len>" or "ERR <code>"; exit 0 only
 * after OK. name is appended when path is an existing folder.
 */
static inline int host_append_copy_put_command(char *command, size_t command_size,
                                               const char *path, const char *name,
                                               unsigned long len, int force)
{
    static char utf8_name[HOST_MAX_PATH_LEN];

    if (!host_copy_latin1_to_utf8(name, utf8_name, sizeof(utf8_name))) {
        return 0;
    }
    if (!host_append_literal(command, command_size, "p=") ||
        !host_copy_append_posix_path(command, command_size, path) ||
        !host_append_literal(command, command_size, "; n=") ||
        !host_append_shell_arg(command, command_size, utf8_name, 0) ||
        !host_append_literal(command, command_size,
                             "; if [ -d \"$p\" ]; then [ -n \"$n\" ] || { echo ERR NONAME; exit 3; }; p=\"${p%/}/$n\";"
                             " else case \"$p\" in */) echo ERR NOTDIR; exit 3;; esac; fi;"
                             " if [ -d \"$p\" ]; then echo ERR ISDIR; exit 3; fi;")) {
        return 0;
    }
    if (!force &&
        !host_append_literal(command, command_size,
                             " if [ -e \"$p\" ] || [ -L \"$p\" ]; then echo ERR EXISTS; exit 3; fi;")) {
        return 0;
    }
    if (!host_append_literal(command, command_size,
                             " case \"$p\" in */*) d=\"${p%/*}\"; [ -n \"$d\" ] || d=/;; *) d=.;; esac;"
                             " t=\"$d/.htput.$$\";"
                             " if ! (set -C; : >\"$t\") 2>/dev/null; then echo ERR OPEN; exit 3; fi;"
                             " trap 'rm -f \"$t\"' 0; trap 'exit 1' 1 2 15;"
                             " echo READY; head -c ") ||
        !host_copy_append_number(command, command_size, len) ||
        !host_append_literal(command, command_size,
                             " >\"$t\" 2>/dev/null;"
                             " s=$(wc -c <\"$t\" | tr -d ' ');"
                             " if [ \"$s\" != ") ||
        !host_copy_append_number(command, command_size, len) ||
        !host_append_literal(command, command_size,
                             " ]; then echo ERR SIZE; cat >/dev/null; exit 3; fi;"
                             " if mv -f \"$t\" \"$p\" 2>/dev/null; then echo \"OK $s\"; exit 0; fi;"
                             " echo ERR MOVE; exit 3")) {
        return 0;
    }
    return 1;
}

/* put, Windows host: same protocol as the POSIX command, through PowerShell. */
static inline int host_append_copy_put_command_windows(char *command, size_t command_size,
                                                       const char *path, const char *name,
                                                       unsigned long len, int force)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    if (!host_append_literal(script, sizeof(script), HOST_PS_STOP_ON_ERROR "$p=") ||
        !host_copy_append_ps_path(script, sizeof(script), path) ||
        !host_append_literal(script, sizeof(script), ";$n=") ||
        !host_append_ps_quoted(script, sizeof(script), name) ||
        !host_append_literal(script, sizeof(script), ";$l=") ||
        !host_copy_append_number(script, sizeof(script), len) ||
        !host_append_literal(script, sizeof(script), ";$f=") ||
        !host_append_literal(script, sizeof(script), force ? "1" : "0") ||
        !host_append_literal(script, sizeof(script),
                             ";$e='OPEN';$t=$o=$null;$d=0;$r=$l;$i=[Console]::OpenStandardInput();"
                             "try{$h=$p.EndsWith('\\')-or $p.EndsWith('/');"
                             "$p=[IO.Path]::GetFullPath($p);"
                             "if([IO.Directory]::Exists($p)){if(!$n){$e='NONAME';throw 1};$p=[IO.Path]::Combine($p,$n)}"
                             "elseif($h){$e='NOTDIR';throw 1};"
                             "if([IO.Directory]::Exists($p)){$e='ISDIR';throw 1};"
                             "if(!$f -and [IO.File]::Exists($p)){$e='EXISTS';throw 1};"
                             "$t=[IO.Path]::Combine([IO.Path]::GetDirectoryName($p),'.htput.'+$PID);"
                             "$o=New-Object IO.FileStream($t,'CreateNew','Write');"
                             "[Console]::Out.Write(\"READY`n\");$d=1;$e='WRITE';"
                             "$b=New-Object byte[] 65536;"
                             "while($r -gt 0){$k=$i.Read($b,0,[Math]::Min(65536,$r));if($k -le 0){break};"
                             "$o.Write($b,0,$k);$r-=$k};"
                             "$o.Dispose();$o=$null;if($r){$e='SIZE';throw 1};$e='MOVE';"
                             "Move-Item -Force -LiteralPath $t -Destination $p;"
                             "$t=$null;[Console]::Out.Write(\"OK $l`n\")}"
                             "catch{if($o){try{$o.Dispose()}catch{}};"
                             "if($t){Remove-Item -Force -LiteralPath $t -ErrorAction SilentlyContinue};"
                             "[Console]::Out.Write(\"ERR $e`n\");"
                             "if($d -and $r -gt 0){try{$b=New-Object byte[] 65536;while($i.Read($b,0,65536) -gt 0){}}catch{}};"
                             "exit 3}")) {
        return 0;
    }
    return host_append_ps_encoded_command(command, command_size, script);
}

enum host_copy_reply_kind {
    HOST_COPY_REPLY_INVALID = 0,
    HOST_COPY_REPLY_READY,
    HOST_COPY_REPLY_OK,
    HOST_COPY_REPLY_ERR
};

/*
 * Classify one reply line of a put command: "READY", "OK <bytes>" or
 * "ERR <code>". Carriage returns and trailing blanks are ignored.
 * For OK, *value is the byte count; for ERR, *code points into line.
 */
static inline enum host_copy_reply_kind host_copy_parse_reply(char *line, unsigned long *value,
                                                              const char **code)
{
    size_t len = strlen(line);
    char *end;

    while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
        line[--len] = '\0';
    }
    if (strcmp(line, "READY") == 0) {
        return HOST_COPY_REPLY_READY;
    }
    if (strncmp(line, "OK ", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
        *value = strtoul(line + 3, &end, 10);
        return *end == '\0' ? HOST_COPY_REPLY_OK : HOST_COPY_REPLY_INVALID;
    }
    if (strncmp(line, "ERR ", 4) == 0 && line[4] != '\0') {
        *code = line + 4;
        return HOST_COPY_REPLY_ERR;
    }
    return HOST_COPY_REPLY_INVALID;
}

static inline const char *host_copy_error_text(const char *code)
{
    if (strcmp(code, "OPEN") == 0) {
        return "Cannot create a file in the host folder (missing folder or no permission)";
    }
    if (strcmp(code, "NOTDIR") == 0) {
        return "The host destination ends with a separator but is not an existing folder";
    }
    if (strcmp(code, "NONAME") == 0) {
        return "The file name cannot be used in a host folder; give a full host file name";
    }
    if (strcmp(code, "ISDIR") == 0) {
        return "The host destination is a folder";
    }
    if (strcmp(code, "EXISTS") == 0) {
        return "The host destination already exists (use FORCE to overwrite)";
    }
    if (strcmp(code, "WRITE") == 0) {
        return "Writing the host file failed";
    }
    if (strcmp(code, "SIZE") == 0) {
        return "The host received an incomplete file";
    }
    if (strcmp(code, "MOVE") == 0) {
        return "The host could not move the file into place";
    }
    return "The host reported an error";
}

#endif
