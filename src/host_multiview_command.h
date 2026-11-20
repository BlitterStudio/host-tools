/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_MULTIVIEW_COMMAND_H
#define HOST_MULTIVIEW_COMMAND_H

#include <stddef.h>
#include "host_common.h"
#include "host_powershell.h"

/*
 * Windows: Start-Process on a path or URL opens it with the default
 * handler. It returns at once and fails when no handler is found.
 * target is ISO-8859-1, like the paths from NativeDosOp.
 */
static inline int host_append_multiview_command_windows(char *command, size_t command_size,
                                                        const char *target)
{
    static char script[HOST_MAX_COMMAND_LEN];

    script[0] = '\0';
    return host_append_literal(script, sizeof(script),
                               HOST_PS_STOP_ON_ERROR "Start-Process -FilePath ") &&
           host_append_ps_quoted(script, sizeof(script), target) &&
           host_append_ps_encoded_command(command, command_size, script);
}

#endif
