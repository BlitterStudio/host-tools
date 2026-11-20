/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_SAY_COMMAND_H
#define HOST_SAY_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

/*
 * POSIX hosts: macOS say, then spd-say, espeak-ng and espeak. The text
 * goes through a shell variable, is converted from ISO-8859-1 to UTF-8
 * when iconv is available, and reaches the program after "--" so that
 * text starting with a dash is not read as an option. say takes the
 * text on standard input instead.
 *
 * The speech program runs in the background and the shell waits for it.
 * Closing the session sends SIGTERM to the shell only; the trap passes
 * it on, so the speech stops with the session instead of finishing.
 * Speech Dispatcher keeps speaking a message after its client is gone,
 * so with spd-say the trap also cancels the queue with "spd-say -C".
 */
#define HOST_SAY_POSIX_CONVERT \
    "; if command -v iconv >/dev/null 2>&1; then" \
    " t=$(printf %s \"$t\" | iconv -f ISO-8859-1 -t UTF-8 2>/dev/null || printf %s \"$t\"); fi" \
    "; c=; trap 'kill $p 2>/dev/null; if [ -n \"$c\" ]; then spd-say -C >/dev/null 2>&1; fi; exit 1' 1 2 15"

#define HOST_SAY_POSIX_RUN \
    "; if command -v say >/dev/null 2>&1; then printf %s \"$t\" | say -f - &" \
    " elif command -v spd-say >/dev/null 2>&1; then c=1; spd-say -w -- \"$t\" &" \
    " elif command -v espeak-ng >/dev/null 2>&1; then espeak-ng -- \"$t\" &" \
    " elif command -v espeak >/dev/null 2>&1; then espeak -- \"$t\" &" \
    " else printf 'No host speech program found (say, spd-say, espeak-ng or espeak)\\n'; exit 127; fi" \
    "; p=$!; wait $p"

static inline int host_append_say_command(char *command, size_t command_size, const char *text)
{
    return host_append_literal(command, command_size, "t=") &&
           host_append_shell_arg(command, command_size, text, 0) &&
           host_append_literal(command, command_size, HOST_SAY_POSIX_CONVERT) &&
           host_append_literal(command, command_size, HOST_SAY_POSIX_RUN);
}

/*
 * Windows: System.Speech through PowerShell. Closing the session ends
 * only the command interpreter, not the PowerShell process under it, so
 * the script speaks asynchronously and watches its standard input,
 * which Amiberry closes with the session, and cancels the speech when
 * that input ends.
 */
#define HOST_SAY_PS_HEAD \
    HOST_PS_STOP_ON_ERROR \
    "Add-Type -AssemblyName System.Speech;" \
    "$s=New-Object System.Speech.Synthesis.SpeechSynthesizer;" \
    "$b=New-Object byte[] 1;" \
    "$r=[Console]::OpenStandardInput().ReadAsync($b,0,1);" \
    "$p=$s.SpeakAsync("
#define HOST_SAY_PS_TAIL \
    ");while(-not $p.IsCompleted -and -not $r.IsCompleted){Start-Sleep -Milliseconds 50};" \
    "if(-not $p.IsCompleted){$s.SpeakAsyncCancelAll()};" \
    "$s.Dispose()"

static inline int host_append_say_command_windows(char *command, size_t command_size,
                                                  const char *text)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script), HOST_SAY_PS_HEAD) &&
           host_append_ps_quoted(script, sizeof(script), text) &&
           host_append_literal(script, sizeof(script), HOST_SAY_PS_TAIL) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
