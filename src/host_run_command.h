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
 * Windows: start a program without waiting for it. arguments is the
 * finished argument string (see host_append_cmdline_arg), empty for
 * none. A missing program makes Start-Process fail.
 */
static inline int host_append_run_command_windows(char *command, size_t command_size,
                                                  const char *program, const char *arguments)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "Start-Process -FilePath ") &&
           host_append_ps_quoted(script, sizeof(script), program) &&
           (arguments[0] == '\0' ||
            (host_append_literal(script, sizeof(script), " -ArgumentList ") &&
             host_append_ps_quoted(script, sizeof(script), arguments))) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
