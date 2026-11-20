/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CONFIG_ACCESS_H
#define HOST_CONFIG_ACCESS_H

#include "host_config_command.h"
#include "uae_pragmas.h"

/*
 * Reads and writes Amiberry's configuration through EmuConfigModify (trap 82).
 *
 * Reading is a search over a text copy of the running configuration
 * (currprefs, "option=value" lines):
 *   - mode -1 with just an option name starts a search for that option, or
 *     for every option starting with it when the name ends in '*'. It
 *     returns -1.
 *   - mode N >= 0 returns the next match in the output buffer and -1, or
 *     0 when there is none. Amiberry reports 10 instead of 0 when mode is 0
 *     and nothing matched, which tells "no such option" from "no more".
 *   - A match for a '*' search is the whole "option=value" line, otherwise
 *     just the value.
 * Writing is mode -1 with "option value" pairs; it changes the pending
 * configuration (changed_prefs) and Amiberry applies it on its next frame.
 * It returns 0, or 5 when a known option is given a value it rejects; an
 * unknown option is ignored and still returns 0, so callers search first. The trap
 * never writes anything to the output buffer when setting.
 *
 * EmuConfig (trap 81) is not used: it reads only the lines of the loaded
 * configuration file that Amiberry did not recognise.
 */

#define HOST_CONFIG_SEARCH_START (-1)

#define HOST_CONFIG_FOUND 0
#define HOST_CONFIG_MISSING 1
#define HOST_CONFIG_END 2
#define HOST_CONFIG_FAILED 3

#define HOST_CONFIG_SET_REJECTED 5

static inline int host_config_begin_search(const char *option, long *rc)
{
    *rc = (long)(int)EmuConfigModify(HOST_CONFIG_SEARCH_START, (UBYTE *)option, 0, 0, 0);
    return *rc == -1 ? HOST_CONFIG_FOUND : HOST_CONFIG_FAILED;
}

/*
 * Fetches match number index (0 for the first). reply must hold
 * HOST_CONFIG_REPLY_MAX bytes.
 */
static inline int host_config_next_match(int index, char *reply, size_t reply_size, long *rc)
{
    reply[0] = '\0';
    reply[reply_size - 1] = '\0';
    *rc = (long)(int)EmuConfigModify(index, (UBYTE *)"", 0, (ULONG)(size_t)reply, (ULONG)reply_size);
    if (*rc == -1) {
        return HOST_CONFIG_FOUND;
    }
    if (*rc == 10 && index == 0) {
        return HOST_CONFIG_MISSING;
    }
    if (*rc == 0 && index > 0) {
        return HOST_CONFIG_END;
    }
    return HOST_CONFIG_FAILED;
}

/* Reads the current value of one option into reply. */
static inline int host_config_get(const char *option, char *reply, size_t reply_size, long *rc)
{
    int status = host_config_begin_search(option, rc);

    if (status != HOST_CONFIG_FOUND) {
        return status;
    }
    return host_config_next_match(0, reply, reply_size, rc);
}

/* Applies a line built by host_config_build_set_line; returns Amiberry's code. */
static inline long host_config_apply(const char *line)
{
    return (long)(int)EmuConfigModify(HOST_CONFIG_SEARCH_START, (UBYTE *)line, 0, 0, 0);
}

#endif
