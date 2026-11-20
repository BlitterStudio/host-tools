/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_EDIT_COMMAND_H
#define HOST_EDIT_COMMAND_H

#include <stddef.h>
#include <string.h>
#include "host_common.h"
#include "host_powershell.h"

/*
 * POSIX editor choice. The tool promises a text editor, so the target's
 * own MIME type is not consulted: asking for the default handler of a
 * PNG or PDF would open an image viewer or a PDF reader. Order: the
 * desktop's text/plain association (launched with the file's URI), then
 * VISUAL or EDITOR, then xdg-open as the last resort.
 *
 * The script reports its own failure through the exit status. Programs
 * it starts must not keep the session's pipe, which is closed when the
 * script ends: a write to it would kill them with SIGPIPE. VISUAL,
 * EDITOR and xdg-open can stay alive as long as the editor does, so
 * they run in the background after a check that the program exists;
 * open and gtk-launch return once the editor is started.
 */
#define HOST_EDIT_COMMAND_HEAD "exec >/dev/null 2>&1 </dev/null; "
#define HOST_EDIT_COMMAND_TAIL \
    "; if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then open -t \"$target\";" \
    " else desktop_file=;" \
    " if command -v xdg-mime >/dev/null 2>&1; then desktop_file=$(xdg-mime query default text/plain 2>/dev/null); fi;" \
    " if [ -n \"$target_uri\" ] && [ -n \"$desktop_file\" ] && command -v gtk-launch >/dev/null 2>&1" \
    " && gtk-launch \"$desktop_file\" \"$target_uri\"; then :;" \
    " elif host_editor=${VISUAL:-${EDITOR:-}}; [ -n \"$host_editor\" ] &&" \
    " HOST_EDITOR=\"$host_editor\" sh -c 'set -- $HOST_EDITOR; command -v -- \"$1\" >/dev/null 2>&1' &&" \
    " ( HOST_EDITOR=\"$host_editor\" sh -c 'exec $HOST_EDITOR \"$1\"' sh \"$target\" & ); then :;" \
    " elif command -v xdg-open >/dev/null 2>&1 && ( xdg-open \"$target\" & ); then :;" \
    " else exit 127; fi; fi"

static inline int host_append_edit_command_with_uri(char *command, size_t command_size,
                                                   const char *path, const char *uri)
{
    return host_append_literal(command, command_size, HOST_EDIT_COMMAND_HEAD "target=") &&
           host_append_shell_arg(command, command_size, path, 0) &&
           host_append_literal(command, command_size, "; target_uri=") &&
           (uri[0] == '\0' || host_append_shell_arg(command, command_size, uri, 0)) &&
           host_append_literal(command, command_size, HOST_EDIT_COMMAND_TAIL);
}

static inline int host_append_edit_command(char *command, size_t command_size, const char *path)
{
    static char uri[HOST_FILE_URI_MAX_LEN];
    size_t original_len;

    if (command == NULL || command_size == 0 || path == NULL) {
        return 0;
    }

    original_len = strlen(command);
    if (original_len >= command_size) {
        return 0;
    }

    if (host_path_to_file_uri(path, uri, sizeof(uri))) {
        if (host_append_edit_command_with_uri(command, command_size, path, uri)) {
            return 1;
        }
        command[original_len] = '\0';
    }

    return host_append_edit_command_with_uri(command, command_size, path, "");
}

/*
 * Windows: open the file in Notepad, the editor host-info reports. The
 * path (UTF-8) is passed as a single quoted argument.
 */
static inline int host_append_edit_command_windows(char *command, size_t command_size,
                                                   const char *path)
{
    static char script[HOST_MAX_COMMAND_LEN];
    static char argument[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    argument[0] = '\0';
    return host_append_cmdline_arg(argument, sizeof(argument), path, 0) &&
           host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "Start-Process -FilePath 'notepad.exe' -ArgumentList ") &&
           host_append_ps_quoted_utf8(script, sizeof(script), argument) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
