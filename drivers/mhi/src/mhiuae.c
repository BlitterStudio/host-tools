/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define __USE_SYSBASE

#include <exec/memory.h>
#include <proto/exec.h>

#include "mhiuae.h"
#include "uae_pragmas.h"

static const char decoder_version[] __attribute__((used)) = MHIUAE_LIBRARY_NAME " " VERSION_STR " (" DATE_STR ")";

BOOL mhiuae_open_uae(void)
{
    return InitUAEResource() ? TRUE : FALSE;
}

void mhiuae_close_uae(void)
{
}

static struct MHIUAEPlayer *valid_player(APTR handle)
{
    return (struct MHIUAEPlayer *)handle;
}

APTR i_MHIAllocDecoder(struct Task *task __asm("a0"), ULONG sigmask __asm("d0"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player;
    ULONG host_handle;

    ObtainSemaphore(&base->allocation_lock);
    if (base->allocated_decoders != 0) {
        ReleaseSemaphore(&base->allocation_lock);
        return NULL;
    }

    if (task == NULL) {
        task = FindTask(NULL);
    }

    host_handle = UaeMHIAlloc(task, sigmask);
    if (host_handle == 0) {
        ReleaseSemaphore(&base->allocation_lock);
        return NULL;
    }

    player = AllocVec(sizeof(*player), MEMF_PUBLIC | MEMF_CLEAR);
    if (player == NULL) {
        UaeMHIFree(host_handle);
        ReleaseSemaphore(&base->allocation_lock);
        return NULL;
    }

    player->task = task;
    player->sigmask = sigmask;
    player->host_handle = host_handle;
    player->status = MHIF_STOPPED;
    base->allocated_decoders++;
    ReleaseSemaphore(&base->allocation_lock);
    return player;
}

void i_MHIFreeDecoder(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    if (player == NULL) {
        return;
    }

    ObtainSemaphore(&base->allocation_lock);
    UaeMHIFree(player->host_handle);
    if (base->allocated_decoders > 0) {
        base->allocated_decoders--;
    }
    ReleaseSemaphore(&base->allocation_lock);
    FreeVec(player);
}

BOOL i_MHIQueueBuffer(APTR handle __asm("a3"), APTR buffer __asm("a0"), ULONG size __asm("d0"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);
    ULONG queued;

    (void)base;
    if (player == NULL || buffer == NULL || size == 0) {
        return FALSE;
    }

    queued = UaeMHIQueue(player->host_handle, buffer, size, (ULONG)buffer);
    if (queued && player->task != NULL && player->sigmask != 0) {
        Signal(player->task, player->sigmask);
    }
    return queued ? TRUE : FALSE;
}

APTR i_MHIGetEmpty(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player == NULL) {
        return NULL;
    }
    return (APTR)UaeMHIGetEmpty(player->host_handle);
}

UBYTE i_MHIGetStatus(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player == NULL) {
        return MHIF_STOPPED;
    }
    player->status = (UBYTE)UaeMHIStatus(player->host_handle);
    return player->status;
}

void i_MHIPlay(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player != NULL && UaeMHIPlay(player->host_handle)) {
        player->status = MHIF_PLAYING;
    }
}

void i_MHIStop(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player != NULL && UaeMHIStop(player->host_handle)) {
        player->status = MHIF_STOPPED;
    }
}

void i_MHIPause(APTR handle __asm("a3"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player != NULL && UaeMHIPause(player->host_handle)) {
        player->status = MHIF_PAUSED;
    }
}

ULONG i_MHIQuery(ULONG query __asm("d1"), struct MHIUAEBase *base __asm("a6"))
{
    (void)base;
    switch (query) {
        case MHIQ_CAPABILITIES:
            return (ULONG)MHIUAE_CAPABILITIES;
        case MHIQ_DECODER_NAME:
            return (ULONG)MHIUAE_DECODER_NAME;
        case MHIQ_DECODER_VERSION:
            return (ULONG)decoder_version;
        case MHIQ_AUTHOR:
            return (ULONG)MHIUAE_AUTHOR;
        case MHIQ_IS_HARDWARE:
            return MHIF_TRUE;
        case MHIQ_IS_68K:
        case MHIQ_IS_PPC:
            return MHIF_FALSE;
        case MHIQ_MPEG1:
        case MHIQ_MPEG2:
        case MHIQ_MPEG25:
        case MHIQ_LAYER3:
        case MHIQ_VARIABLE_BITRATE:
        case MHIQ_JOINT_STEREO:
        case MHIQ_VOLUME_CONTROL:
        case MHIQ_PANNING_CONTROL:
            return MHIF_SUPPORTED;
        default:
            return MHIF_UNSUPPORTED;
    }
}

void i_MHISetParam(APTR handle __asm("a3"), UWORD param __asm("d0"), ULONG value __asm("d1"), struct MHIUAEBase *base __asm("a6"))
{
    struct MHIUAEPlayer *player = valid_player(handle);

    (void)base;
    if (player != NULL) {
        UaeMHISetParam(player->host_handle, param, value);
    }
}
