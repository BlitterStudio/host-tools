/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_RUN_COMMAND_H
#define HOST_RUN_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

/*
 * POSIX: start a program without waiting for it. program and arguments
 * are shell words (see host_append_shell_arg); arguments is empty or
 * starts with a space. The script exits 127 with a message when the
 * program is not on the PATH, or is not an executable file when given
 * as a path, and otherwise starts it detached from the
 * pipe session and exits 0. The program's input and output go to
 * /dev/null: the session only ends when every holder of its pipe has
 * closed it, so a program that kept it would stall the script's exit
 * until the program quits.
 */
static inline int host_append_run_command(char *command, size_t command_size,
                                          const char *program, const char *arguments)
{
    return host_append_literal(command, command_size, "p=") &&
           host_append_literal(command, command_size, program) &&
           host_append_literal(command, command_size,
                               "; case $p in */*) [ -f \"$p\" ] && [ -x \"$p\" ];;"
                               " *) command -v -- \"$p\" >/dev/null 2>&1;; esac || "
                               "{ printf 'Command not found on host: %s\\n' \"$p\"; exit 127; }; "
                               "( \"$p\"") &&
           host_append_literal(command, command_size, arguments) &&
           host_append_literal(command, command_size,
                               " >/dev/null 2>&1 </dev/null & ); exit 0");
}

/*
 * Windows: start a program without waiting for it. program and
 * arguments are UTF-8. arguments is the finished argument string (see
 * host_append_cmd_word and host_append_cmdline_arg), empty for none. A
 * missing program makes Start-Process fail.
 *
 * Windows starts .bat and .cmd files through cmd.exe, where quotes and
 * percent signs in an argument cannot be made safe. batch_safe says
 * the arguments hold neither; when it is 0 the script refuses to run a
 * batch file with them and still runs native programs.
 */
static inline int host_append_run_command_windows(char *command, size_t command_size,
                                                  const char *program, const char *arguments,
                                                  int batch_safe)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    if (batch_safe) {
        return host_append_literal(script, sizeof(script),
                                   HOST_PS_STOP_ON_ERROR "Start-Process -FilePath ") &&
               host_append_ps_quoted_utf8(script, sizeof(script), program) &&
               (arguments[0] == '\0' ||
                (host_append_literal(script, sizeof(script), " -ArgumentList ") &&
                 host_append_ps_quoted_utf8(script, sizeof(script), arguments))) &&
               host_append_ps_encoded_command(command, command_size, script);
    }

    return host_append_literal(script, sizeof(script), HOST_PS_STOP_ON_ERROR "$f=") &&
           host_append_ps_quoted_utf8(script, sizeof(script), program) &&
           host_append_literal(script, sizeof(script),
                               ";$e=[IO.Path]::GetExtension($f);"
                               "if(-not $e){$c=Get-Command -Name $f -CommandType Application"
                               " -ErrorAction SilentlyContinue|Select-Object -First 1;"
                               "if($c){$e=$c.Extension}};"
                               "if($e -match '^\\.(bat|cmd)$'){throw 'Arguments with quotes or"
                               " percent signs cannot be passed to a batch file'};"
                               "Start-Process -FilePath $f -ArgumentList ") &&
           host_append_ps_quoted_utf8(script, sizeof(script), arguments) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
