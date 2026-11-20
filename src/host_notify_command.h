/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_NOTIFY_COMMAND_H
#define HOST_NOTIFY_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

static inline int host_append_notify_command(char *command, size_t command_size,
                                             const char *title, const char *message)
{
    return host_append_literal(command, command_size, "t=") &&
           host_append_shell_arg(command, command_size, title, 0) &&
           host_append_literal(command, command_size, "; m=") &&
           host_append_shell_arg(command, command_size, message, 0) &&
           host_append_literal(command, command_size,
                               "; if command -v iconv >/dev/null 2>&1; then"
                               " t=$(printf %s \"$t\" | iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || printf %s \"$t\");"
                               " m=$(printf %s \"$m\" | iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || printf %s \"$m\"); fi"
                               "; if command -v notify-send >/dev/null 2>&1; then notify-send -- \"$t\" \"$m\""
                               /* osascript reads the script from standard input ("-"), which ends its option parsing, so a title or message starting with '-' stays an argument */
                               "; elif command -v osascript >/dev/null 2>&1; then printf '%s\\n' 'on run argv' 'display notification (item 2 of argv) with title (item 1 of argv)' 'end run' | osascript - \"$t\" \"$m\""
                               "; else exit 127; fi");
}

/*
 * Windows toast notification through the WinRT API. The title and
 * message become text nodes, so they need no XML escaping. The toast is
 * attributed to Windows PowerShell, whose application id is registered
 * with the notification system.
 */
#define HOST_NOTIFY_PS_HEAD \
    HOST_PS_STOP_ON_ERROR \
    "$m=[Windows.UI.Notifications.ToastNotificationManager,Windows.UI.Notifications,ContentType=WindowsRuntime];" \
    "$x=$m::GetTemplateContent([Windows.UI.Notifications.ToastTemplateType]::ToastText02);" \
    "$e=$x.GetElementsByTagName('text');" \
    "[void]$e.Item(0).AppendChild($x.CreateTextNode("
#define HOST_NOTIFY_PS_MIDDLE \
    "));[void]$e.Item(1).AppendChild($x.CreateTextNode("
#define HOST_NOTIFY_PS_TAIL \
    "));$m::CreateToastNotifier('{1AC14E77-02E7-4E5D-B744-2EB1AE5198B7}\\WindowsPowerShell\\v1.0\\powershell.exe')" \
    ".Show([Windows.UI.Notifications.ToastNotification]::new($x))"

static inline int host_append_notify_command_windows(char *command, size_t command_size,
                                                     const char *title, const char *message)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script), HOST_NOTIFY_PS_HEAD) &&
           host_append_ps_quoted(script, sizeof(script), title) &&
           host_append_literal(script, sizeof(script), HOST_NOTIFY_PS_MIDDLE) &&
           host_append_ps_quoted(script, sizeof(script), message) &&
           host_append_literal(script, sizeof(script), HOST_NOTIFY_PS_TAIL) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
