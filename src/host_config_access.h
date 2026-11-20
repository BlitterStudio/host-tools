/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_CONFIG_ACCESS_H
#define HOST_CONFIG_ACCESS_H

#include <exec/memory.h>
#include <exec/semaphores.h>

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
 * unknown option is ignored and still returns 0, so callers search first.
 * The trap never writes anything to the output buffer when setting.
 *
 * Amiberry keeps the search text and position in one static, shared by every
 * caller. A search is therefore a sequence that no other task may enter, and
 * these tools hold a named public semaphore for the whole of it: from
 * host_config_begin_search to the last host_config_next_match.
 *
 * EmuConfig (trap 81) is not used: it reads only the lines of the loaded
 * configuration file that Amiberry did not recognise.
 */

#define HOST_CONFIG_SEARCH_START (-1)

#define HOST_CONFIG_FOUND 0
#define HOST_CONFIG_MISSING 1
#define HOST_CONFIG_END 2
#define HOST_CONFIG_FAILED 3
#define HOST_CONFIG_BUSY 4

#define HOST_CONFIG_SET_REJECTED 5

/* The semaphore is created by the first tool to run and stays for the session. */
#define HOST_CONFIG_LOCK_NAME "host-tools.config"
#define HOST_CONFIG_LOCK_NAME_MAX 24
/* Another tool holds a search only for the length of a few traps. */
#define HOST_CONFIG_LOCK_TICKS 5
#define HOST_CONFIG_LOCK_POLLS 50

struct host_config_lock {
    struct SignalSemaphore semaphore;
    char name[HOST_CONFIG_LOCK_NAME_MAX];
};

static struct SignalSemaphore *host_config_held;

static inline struct SignalSemaphore *host_config_find_lock(void)
{
    struct SignalSemaphore *semaphore;
    struct host_config_lock *created;

    Forbid();
    semaphore = FindSemaphore((STRPTR)HOST_CONFIG_LOCK_NAME);
    Permit();
    if (semaphore != NULL) {
        return semaphore;
    }

    created = (struct host_config_lock *)AllocMem(sizeof(*created), MEMF_PUBLIC | MEMF_CLEAR);
    if (created == NULL) {
        return NULL;
    }
    memcpy(created->name, HOST_CONFIG_LOCK_NAME, sizeof(HOST_CONFIG_LOCK_NAME));
    InitSemaphore(&created->semaphore);
    created->semaphore.ss_Link.ln_Name = created->name;

    /* another task may have created it while this one allocated */
    Forbid();
    semaphore = FindSemaphore((STRPTR)HOST_CONFIG_LOCK_NAME);
    if (semaphore == NULL) {
        AddSemaphore(&created->semaphore);
        semaphore = &created->semaphore;
        created = NULL;
    }
    Permit();
    if (created != NULL) {
        FreeMem(created, sizeof(*created));
    }
    return semaphore;
}

/* Takes the search semaphore, waiting up to about five seconds. Returns 0 on timeout. */
static inline int host_config_lock(void)
{
    struct SignalSemaphore *semaphore = host_config_find_lock();

    if (semaphore == NULL) {
        return 0;
    }
    for (int poll = 0; poll < HOST_CONFIG_LOCK_POLLS; poll++) {
        if (AttemptSemaphore(semaphore)) {
            host_config_held = semaphore;
            return 1;
        }
        Delay(HOST_CONFIG_LOCK_TICKS);
    }
    return 0;
}

static inline void host_config_unlock(void)
{
    if (host_config_held != NULL) {
        struct SignalSemaphore *semaphore = host_config_held;

        host_config_held = NULL;
        ReleaseSemaphore(semaphore);
    }
}

/* The caller holds the semaphore. */
static inline int host_config_begin_search(const char *option, long *rc)
{
    *rc = (long)EmuConfigModify(HOST_CONFIG_SEARCH_START, (UBYTE *)option, 0, 0, 0);
    return *rc == -1 ? HOST_CONFIG_FOUND : HOST_CONFIG_FAILED;
}

/*
 * Fetches match number index (0 for the first). reply must hold
 * HOST_CONFIG_REPLY_MAX bytes. The caller holds the semaphore.
 */
static inline int host_config_next_match(int index, char *reply, size_t reply_size, long *rc)
{
    reply[0] = '\0';
    reply[reply_size - 1] = '\0';
    *rc = (long)EmuConfigModify(index, (UBYTE *)"", 0, (ULONG)(size_t)reply, (ULONG)reply_size);
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

/* Reads the current value of one option into reply. Takes the semaphore itself. */
static inline int host_config_get(const char *option, char *reply, size_t reply_size, long *rc)
{
    int status;

    if (!host_config_lock()) {
        *rc = 0;
        return HOST_CONFIG_BUSY;
    }
    status = host_config_begin_search(option, rc);
    if (status == HOST_CONFIG_FOUND) {
        status = host_config_next_match(0, reply, reply_size, rc);
    }
    host_config_unlock();
    return status;
}

/* Applies a line built by host_config_build_set_line; returns Amiberry's code. */
static inline long host_config_apply(const char *line)
{
    return (long)EmuConfigModify(HOST_CONFIG_SEARCH_START, (UBYTE *)line, 0, 0, 0);
}

#define HOST_CONFIG_BUSY_TEXT "Another host tool is reading Amiberry's configuration; try again"

#endif
