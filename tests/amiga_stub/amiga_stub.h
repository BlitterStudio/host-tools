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
typedef const UBYTE *CONST_STRPTR;
typedef ULONG BPTR;

struct Library {
    int lib_dummy;
};

struct Node {
    char *ln_Name;
};

struct Task {
    int tc_dummy;
};

struct Process {
    APTR pr_WindowPtr;
};

struct SignalSemaphore {
    struct Node ss_Link;
    int ss_Held; /* the stub's own bookkeeping, not an exec field */
};

struct FileInfoBlock {
    long fib_DirEntryType;
};

struct DosList {
    int dl_dummy;
};

#define ACCESS_READ (-2L)
#define SIGBREAKF_CTRL_C (1UL << 12)
#define MEMF_PUBLIC 1UL
#define MEMF_CLEAR 2UL
#define DOS_FIB 2UL
#define LDF_DEVICES 4UL
#define LDF_VOLUMES 8UL
#define LDF_ASSIGNS 16UL
#define LDF_READ 1UL

struct Task *FindTask(const char *name);
APTR OpenResource(UBYTE *name);
ULONG SetSignal(ULONG new_signals, ULONG signal_set);
BPTR Lock(STRPTR name, long mode);
void UnLock(BPTR lock);
void Delay(long ticks);
long IoErr(void);
long Examine(BPTR lock, struct FileInfoBlock *fib);
APTR AllocDosObject(ULONG type, void *tags);
void FreeDosObject(ULONG type, APTR object);
struct DosList *LockDosList(ULONG flags);
struct DosList *FindDosEntry(struct DosList *list, CONST_STRPTR name, ULONG flags);
void UnLockDosList(ULONG flags);

APTR AllocMem(ULONG size, ULONG flags);
void FreeMem(APTR memory, ULONG size);
void Forbid(void);
void Permit(void);
void InitSemaphore(struct SignalSemaphore *semaphore);
void AddSemaphore(struct SignalSemaphore *semaphore);
struct SignalSemaphore *FindSemaphore(CONST_STRPTR name);
long AttemptSemaphore(struct SignalSemaphore *semaphore);
void ReleaseSemaphore(struct SignalSemaphore *semaphore);

#endif
