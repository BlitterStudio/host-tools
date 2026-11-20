/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * Just enough of the AmigaOS types and calls for src/uae_pragmas.h and
 * src/host_path.h to compile on the build host. tests/fake_uae.h implements
 * them. ULONG is as wide as a pointer so that the pointers the tools pass to
 * the traps survive the trip through ULONG.
 */

#ifndef AMIGA_STUB_H
#define AMIGA_STUB_H

#include <stddef.h>

typedef unsigned char UBYTE;
typedef signed char BYTE;
typedef unsigned short UWORD;
typedef size_t ULONG;
typedef void *APTR;
typedef UBYTE *STRPTR;
typedef ULONG BPTR;

struct Library {
    int lib_dummy;
};

struct Task {
    int tc_dummy;
};

struct Process {
    APTR pr_WindowPtr;
};

#define ACCESS_READ (-2L)
#define SIGBREAKF_CTRL_C (1UL << 12)

struct Task *FindTask(const char *name);
APTR OpenResource(UBYTE *name);
ULONG SetSignal(ULONG new_signals, ULONG signal_set);
BPTR Lock(STRPTR name, long mode);
void UnLock(BPTR lock);
void Delay(long ticks);

#endif
