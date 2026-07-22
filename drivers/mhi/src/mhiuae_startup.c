/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define __USE_SYSBASE

#include <stddef.h>
#include <stdint.h>

#include <exec/execbase.h>
#include <exec/initializers.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <proto/exec.h>

#include "mhiuae.h"

struct ExecBase *SysBase = NULL;

extern APTR FuncTab[];
extern struct MyDataInit DataTab;
extern APTR EndResident;

struct MHIUAEBase *InitLib(struct ExecBase *sysbase __asm("a6"), BPTR seglist __asm("a0"), struct MHIUAEBase *base __asm("d0"));
struct MHIUAEBase *OpenLib(struct MHIUAEBase *base __asm("a6"));
BPTR CloseLib(struct MHIUAEBase *base __asm("a6"));
BPTR ExpungeLib(struct MHIUAEBase *base __asm("a6"));
ULONG ExtFuncLib(void);

static const char lib_name[] __attribute__((used)) = MHIUAE_LIBRARY_NAME;
static const char lib_id[] __attribute__((used)) = MHIUAE_LIBRARY_NAME " " VERSION_STR " (" DATE_STR ")";
static const char ver_string[] __attribute__((used)) = "\0$VER: " MHIUAE_LIBRARY_NAME " " VERSION_STR " (" DATE_STR ")";

#define MHIUAE_INIT_PTR(value) ((ULONG)(uintptr_t)(value))

LONG LibStart(void)
{
    return -1;
}

struct InitTable {
    ULONG lib_base_size;
    APTR *function_table;
    struct MyDataInit *data_table;
    APTR init_function;
};

struct InitTable InitTab = {
    sizeof(struct MHIUAEBase),
    &FuncTab[0],
    &DataTab,
    InitLib
};

APTR FuncTab[] = {
    OpenLib,
    CloseLib,
    ExpungeLib,
    ExtFuncLib,
    i_MHIAllocDecoder,
    i_MHIFreeDecoder,
    i_MHIQueueBuffer,
    i_MHIGetEmpty,
    i_MHIGetStatus,
    i_MHIPlay,
    i_MHIStop,
    i_MHIPause,
    i_MHIQuery,
    i_MHISetParam,
    (APTR)((LONG)-1)
};

struct Resident ROMTag __attribute__((used)) = {
    RTC_MATCHWORD,
    &ROMTag,
    &EndResident,
    RTF_AUTOINIT,
    LIB_VERSION,
    NT_LIBRARY,
    0,
    (char *)&lib_name[0],
    (char *)&lib_id[0],
    &InitTab
};

APTR EndResident;

struct MyDataInit {
    UWORD ln_Type_Init;      UWORD ln_Type_Offset;      UWORD ln_Type_Content;
    UBYTE ln_Name_Init;      UBYTE ln_Name_Offset;      ULONG ln_Name_Content;
    UWORD lib_Flags_Init;    UWORD lib_Flags_Offset;    UWORD lib_Flags_Content;
    UWORD lib_Version_Init;  UWORD lib_Version_Offset;  UWORD lib_Version_Content;
    UWORD lib_Revision_Init; UWORD lib_Revision_Offset; UWORD lib_Revision_Content;
    UBYTE lib_IdString_Init; UBYTE lib_IdString_Offset; ULONG lib_IdString_Content;
    ULONG end_mark;
} DataTab = {
    INITBYTE(OFFSET(Node, ln_Type), NT_LIBRARY),
    0x80, (UBYTE)offsetof(struct Node, ln_Name), MHIUAE_INIT_PTR(&lib_name[0]),
    INITBYTE(OFFSET(Library, lib_Flags), LIBF_SUMUSED | LIBF_CHANGED),
    INITWORD(OFFSET(Library, lib_Version), LIB_VERSION),
    INITWORD(OFFSET(Library, lib_Revision), LIB_REVISION),
    0x80, (UBYTE)offsetof(struct Library, lib_IdString), MHIUAE_INIT_PTR(&lib_id[0]),
    0
};

struct MHIUAEBase *InitLib(struct ExecBase *sysbase __asm("a6"), BPTR seglist __asm("a0"), struct MHIUAEBase *base __asm("d0"))
{
    ULONG negsize;
    ULONG possize;
    ULONG fullsize;
    UBYTE *negptr;

    SysBase = sysbase;
    base->sys_base = sysbase;
    base->seg_list = seglist;
    InitSemaphore(&base->allocation_lock);
    base->allocated_decoders = 0;

    if (mhiuae_open_uae()) {
        return base;
    }

    negsize = base->lib.lib_NegSize;
    possize = base->lib.lib_PosSize;
    fullsize = negsize + possize;
    negptr = (UBYTE *)base - negsize;
    FreeMem(negptr, fullsize);
    return NULL;
}

struct MHIUAEBase *OpenLib(struct MHIUAEBase *base __asm("a6"))
{
    base->lib.lib_OpenCnt++;
    base->lib.lib_Flags &= ~LIBF_DELEXP;
    return base;
}

BPTR CloseLib(struct MHIUAEBase *base __asm("a6"))
{
    if (base->lib.lib_OpenCnt > 0) {
        base->lib.lib_OpenCnt--;
    }
    if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP)) {
        return ExpungeLib(base);
    }
    return 0;
}

BPTR ExpungeLib(struct MHIUAEBase *base __asm("a6"))
{
    BPTR seglist;
    ULONG negsize;
    ULONG possize;
    ULONG fullsize;
    UBYTE *negptr;

    if (base->lib.lib_OpenCnt != 0 || base->allocated_decoders != 0) {
        base->lib.lib_Flags |= LIBF_DELEXP;
        return 0;
    }

    seglist = base->seg_list;
    Remove((struct Node *)base);
    mhiuae_close_uae();

    negsize = base->lib.lib_NegSize;
    possize = base->lib.lib_PosSize;
    fullsize = negsize + possize;
    negptr = (UBYTE *)base - negsize;
    FreeMem(negptr, fullsize);
    return seglist;
}

ULONG ExtFuncLib(void)
{
    return 0;
}
