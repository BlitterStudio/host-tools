/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MHIUAE_H
#define MHIUAE_H

#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <exec/tasks.h>
#include <exec/types.h>

#include "mhi_abi.h"

#define MHIUAE_LIBRARY_NAME "mhiuae.library"
#define MHIUAE_DECODER_NAME "UAE MP3 decoder"
#define MHIUAE_AUTHOR "Amiberry"
#define MHIUAE_CAPABILITIES "audio/mpeg audio/mp3"

struct MHIUAEBase {
    struct Library lib;
    BPTR seg_list;
    struct ExecBase *sys_base;
    struct SignalSemaphore allocation_lock;
    ULONG allocated_decoders;
};

struct MHIUAEPlayer {
    ULONG host_handle;
    UBYTE status;
};

BOOL mhiuae_open_uae(void);
void mhiuae_close_uae(void);

APTR i_MHIAllocDecoder(struct Task *task __asm("a0"), ULONG sigmask __asm("d0"), struct MHIUAEBase *base __asm("a6"));
void i_MHIFreeDecoder(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
BOOL i_MHIQueueBuffer(APTR handle __asm("a3"), APTR buffer __asm("a0"), ULONG size __asm("d0"), struct MHIUAEBase *base __asm("a6"));
APTR i_MHIGetEmpty(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
UBYTE i_MHIGetStatus(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
void i_MHIPlay(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
void i_MHIStop(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
void i_MHIPause(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"));
ULONG i_MHIQuery(ULONG query __asm("d1"), struct MHIUAEBase *base __asm("a6"));
void i_MHISetParam(APTR handle __asm("a3"), UWORD param __asm("d0"), ULONG value __asm("d1"), struct MHIUAEBase *base __asm("a6"));

#endif
