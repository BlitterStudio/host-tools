/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <devices/ahi.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <exec/execbase.h>
#include <exec/interrupts.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/resident.h>
#include <hardware/intbits.h>
#include <libraries/ahi_sub.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <utility/hooks.h>
#include <utility/utility.h>

#ifndef VERSION_STR
#define VERSION_STR "2.3"
#endif

#ifndef DATE_STR
#define DATE_STR "2026-04-30"
#endif

#define UAE_DRIVER_VERSION 2
#define UAE_DRIVER_REVISION 3
#define UAE_UNIT 0
#define UAE_CHANNELS 2
#define UAE_BITS 16
#define UAE_MAX_AHI_CHANNELS 128
#define UAE_BUFFER_BLOCKS 2
#define UAE_DEFAULT_BLOCK_SAMPLES 1024

#ifndef UTILITYNAME
#define UTILITYNAME "utility.library"
#endif

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *UtilityBase;

struct UAEBase;

extern APTR _etext;

extern APTR gwLibInit(void);
extern APTR gwLibOpen(void);
extern APTR gwLibClose(void);
extern APTR gwLibExpunge(void);
extern APTR gwLibNull(void);
extern APTR gwAHIsub_AllocAudio(void);
extern APTR gwAHIsub_FreeAudio(void);
extern APTR gwAHIsub_Disable(void);
extern APTR gwAHIsub_Enable(void);
extern APTR gwAHIsub_Start(void);
extern APTR gwAHIsub_Update(void);
extern APTR gwAHIsub_Stop(void);
extern APTR gwAHIsub_SetVol(void);
extern APTR gwAHIsub_SetFreq(void);
extern APTR gwAHIsub_SetSound(void);
extern APTR gwAHIsub_SetEffect(void);
extern APTR gwAHIsub_LoadSound(void);
extern APTR gwAHIsub_UnloadSound(void);
extern APTR gwAHIsub_GetAttr(void);
extern APTR gwAHIsub_HardwareControl(void);
extern void ahi_interrupt_entry(void);
extern void ahi_softint_entry(void);

extern LONG uae_ahi_open(ULONG trap_addr, ULONG unit, ULONG freq, ULONG block_samples, ULONG channels, ULONG bits);
extern LONG uae_ahi_close(ULONG trap_addr, ULONG unit);
extern LONG uae_ahi_write(ULONG trap_addr, ULONG unit, APTR buffer);
extern LONG uae_ahi_write_interrupt(ULONG trap_addr, ULONG unit);
extern LONG uae_ahi_poll(ULONG trap_addr);
extern ULONG uae_find_trap(APTR resource, APTR name);
extern BOOL ahi_call_pre_timer(APTR timer, struct AHIAudioCtrlDrv *audioctrl);
extern void ahi_call_post_timer(APTR timer, struct AHIAudioCtrlDrv *audioctrl);

ULONG UAE_AHIsub_Stop(ULONG flags, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base);

static const char LibName[] = "uae.audio";
static const char LibIDString[] = "\0$VER: uae.audio " VERSION_STR " (" DATE_STR ")";
static const char AuthorString[] = "Host-Tools";
static const char CopyrightString[] = "Copyright 2020-2026 Dimitris Panokostas";
static const char OutputString[] = "UAE";
static const char UaeResourceName[] = "uae.resource";
static const char AhiTrapName[] = "ahi_winuae";

static const LONG Frequencies[] = {
	5513,
	8000,
	9600,
	10000,
	11025,
	12000,
	14700,
	16000,
	17640,
	18900,
	19200,
	20000,
	22050,
	24000,
	27429,
	29400,
	31968,
	32000,
	32032,
	33075,
	37800,
	44056,
	44100,
	44144,
	47952,
	48000,
	48048,
	88200,
	96000
};

#define FREQUENCY_COUNT ((LONG)(sizeof(Frequencies) / sizeof(Frequencies[0])))

struct UAEBase
{
	struct Library library;
	BPTR seglist;
	struct ExecBase *sysbase;
	struct DosLibrary *dosbase;
	struct Library *utilitybase;
	ULONG trap_addr;
};

struct UAEResourceBase
{
	struct Library uae_lib;
	UWORD uae_version;
	UWORD uae_revision;
	UWORD uae_subrevision;
	UWORD zero;
	APTR uae_rombase;
};

struct UAEAudioData
{
	struct UAEBase *base;
	struct AHIAudioCtrlDrv *audioctrl;
	struct Interrupt playinterrupt;
	struct Interrupt softinterrupt;
	APTR playbuffers[2];
	APTR mixbuffers[2];
	APTR softintbuffer;
	ULONG mixbuffsize;
	ULONG hostblocksize;
	ULONG hostbuffersize;
	ULONG blocksamples;
	ULONG hostblocksamples;
	ULONG mixfreq;
	UBYTE activebuffer;
	BOOL interrupt_added;
	volatile UWORD disable_count;
	volatile BOOL softint_active;
	BOOL running;
};

static const APTR FuncTable[] = {
	(APTR)gwLibOpen,
	(APTR)gwLibClose,
	(APTR)gwLibExpunge,
	(APTR)gwLibNull,
	(APTR)gwAHIsub_AllocAudio,
	(APTR)gwAHIsub_FreeAudio,
	(APTR)gwAHIsub_Disable,
	(APTR)gwAHIsub_Enable,
	(APTR)gwAHIsub_Start,
	(APTR)gwAHIsub_Update,
	(APTR)gwAHIsub_Stop,
	(APTR)gwAHIsub_SetVol,
	(APTR)gwAHIsub_SetFreq,
	(APTR)gwAHIsub_SetSound,
	(APTR)gwAHIsub_SetEffect,
	(APTR)gwAHIsub_LoadSound,
	(APTR)gwAHIsub_UnloadSound,
	(APTR)gwAHIsub_GetAttr,
	(APTR)gwAHIsub_HardwareControl,
	(APTR)-1
};

static const APTR InitTable[] = {
	(APTR)sizeof(struct UAEBase),
	(APTR)FuncTable,
	NULL,
	(APTR)gwLibInit
};

const struct Resident RomTag __attribute__((used)) = {
	RTC_MATCHWORD,
	(struct Resident *)&RomTag,
	(struct Resident *)&_etext,
	RTF_AUTOINIT,
	UAE_DRIVER_VERSION,
	NT_LIBRARY,
	0,
	(APTR)LibName,
	(APTR)(LibIDString + 1),
	(APTR)InitTable
};

int _start(void)
{
	return -1;
}

static ULONG find_ahi_trap(void)
{
	struct UAEResourceBase *uae;
	ULONG trap_addr;

	uae = (struct UAEResourceBase *)OpenResource((UBYTE *)UaeResourceName);
	if (uae != NULL) {
		trap_addr = uae_find_trap((APTR)uae, (APTR)AhiTrapName);
		if (trap_addr != 0) {
			return trap_addr;
		}
	}

	return 0xf0ffc0;
}

struct UAEBase *UAE_LibInit(struct UAEBase *base, BPTR seglist, struct ExecBase *sysbase)
{
	SysBase = sysbase;
	DOSBase = NULL;
	UtilityBase = NULL;

	base->library.lib_Node.ln_Type = NT_LIBRARY;
	base->library.lib_Node.ln_Name = (APTR)LibName;
	base->library.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	base->library.lib_Version = UAE_DRIVER_VERSION;
	base->library.lib_Revision = UAE_DRIVER_REVISION;
	base->library.lib_IdString = (STRPTR)LibIDString + 1;
	base->seglist = seglist;
	base->sysbase = sysbase;
	base->trap_addr = find_ahi_trap();

	if (base->trap_addr == 0) {
		return NULL;
	}

	DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)DOSNAME, 37);
	if (DOSBase == NULL) {
		return NULL;
	}

	UtilityBase = OpenLibrary((CONST_STRPTR)UTILITYNAME, 37);
	if (UtilityBase == NULL) {
		CloseLibrary((struct Library *)DOSBase);
		DOSBase = NULL;
		return NULL;
	}

	base->dosbase = DOSBase;
	base->utilitybase = UtilityBase;
	return base;
}

BPTR UAE_LibExpunge(struct UAEBase *base)
{
	BPTR seglist;
	ULONG size;
	UBYTE *memory;

	if (base->library.lib_OpenCnt != 0) {
		base->library.lib_Flags |= LIBF_DELEXP;
		return 0;
	}

	seglist = base->seglist;
	if (base->library.lib_Node.ln_Succ != NULL) {
		Remove((struct Node *)base);
	}

	if (base->utilitybase != NULL) {
		CloseLibrary(base->utilitybase);
	}
	if (base->dosbase != NULL) {
		CloseLibrary((struct Library *)base->dosbase);
	}

	memory = (UBYTE *)base - base->library.lib_NegSize;
	size = base->library.lib_NegSize + base->library.lib_PosSize;
	FreeMem(memory, size);

	return seglist;
}

struct UAEBase *UAE_LibOpen(ULONG version, struct UAEBase *base)
{
	(void)version;

	base->library.lib_Flags &= (UBYTE)~LIBF_DELEXP;
	base->library.lib_OpenCnt++;
	return base;
}

BPTR UAE_LibClose(struct UAEBase *base)
{
	base->library.lib_OpenCnt--;
	if (base->library.lib_OpenCnt == 0 && (base->library.lib_Flags & LIBF_DELEXP) != 0) {
		return UAE_LibExpunge(base);
	}
	return 0;
}

ULONG UAE_LibNull(struct UAEBase *base)
{
	(void)base;
	return 0;
}

static LONG closest_frequency_index(LONG frequency)
{
	LONG i;

	if (frequency <= Frequencies[0]) {
		return 0;
	}

	for (i = 1; i < FREQUENCY_COUNT; i++) {
		if (frequency <= Frequencies[i]) {
			LONG lower = Frequencies[i - 1];
			LONG upper = Frequencies[i];
			return (frequency - lower < upper - frequency) ? i - 1 : i;
		}
	}

	return FREQUENCY_COUNT - 1;
}

static void clear_buffer(APTR buffer, ULONG size)
{
	UBYTE *dst;
	ULONG i;

	dst = (UBYTE *)buffer;
	for (i = 0; i < size; i++) {
		dst[i] = 0;
	}
}

static void convert_hifi_to_host(APTR source, APTR destination, ULONG samples)
{
	WORD *src;
	WORD *dst;
	ULONG i;

	src = (WORD *)source;
	dst = (WORD *)destination;
	for (i = 0; i < samples; i++) {
		dst[0] = src[0];
		dst[1] = src[2];
		src += 4;
		dst += 2;
	}
}

static void mix_playback_buffer(struct UAEAudioData *data, APTR buffer)
{
	struct AHIAudioCtrlDrv *audioctrl = data->audioctrl;
	UBYTE *hostdst = (UBYTE *)buffer;
	UBYTE *mixdst = (UBYTE *)data->mixbuffers[data->activebuffer ^ 1];
	BOOL skip_mix = FALSE;
	BOOL hifi = (audioctrl->ahiac_Flags & AHIACF_HIFI) != 0;
	ULONG i;

	if (audioctrl->ahiac_PreTimer != NULL) {
		skip_mix = ahi_call_pre_timer((APTR)audioctrl->ahiac_PreTimer, audioctrl);
	}

	for (i = 0; i < UAE_BUFFER_BLOCKS; i++) {
		APTR hostblock = hostdst + (data->hostblocksize * i);
		APTR mixblock = hifi ? (APTR)(mixdst + (data->mixbuffsize * i)) : hostblock;

		if (audioctrl->ahiac_PlayerFunc != NULL) {
			CallHookPkt(audioctrl->ahiac_PlayerFunc, audioctrl, NULL);
		}

		if (skip_mix) {
			clear_buffer(hostblock, data->hostblocksize);
		} else if (audioctrl->ahiac_MixerFunc != NULL) {
			CallHookPkt(audioctrl->ahiac_MixerFunc, audioctrl, mixblock);
			if (hifi) {
				convert_hifi_to_host(mixblock, hostblock, data->blocksamples);
			}
		} else {
			clear_buffer(hostblock, data->hostblocksize);
		}
	}

	if (audioctrl->ahiac_PostTimer != NULL) {
		ahi_call_post_timer((APTR)audioctrl->ahiac_PostTimer, audioctrl);
	}
}

ULONG UAE_AHIInterrupt(struct UAEAudioData *data)
{
	APTR buffer;

	if (data == NULL || !data->running || data->disable_count != 0 || data->softint_active) {
		return 0;
	}

	if (uae_ahi_write_interrupt(data->base->trap_addr, UAE_UNIT) <= 0) {
		return 0;
	}

	buffer = data->playbuffers[data->activebuffer];
	(void)uae_ahi_write(data->base->trap_addr, UAE_UNIT, buffer);

	data->softintbuffer = buffer;
	data->softint_active = TRUE;
	data->activebuffer ^= 1;
	Cause(&data->softinterrupt);

	return 0;
}

void UAE_AHISoftInt(struct UAEAudioData *data)
{
	APTR buffer;

	if (data == NULL) {
		return;
	}

	buffer = data->softintbuffer;
	data->softintbuffer = NULL;

	if (data->running && buffer != NULL) {
		mix_playback_buffer(data, buffer);
	}

	data->softint_active = FALSE;
}

ULONG UAE_AHIsub_AllocAudio(struct TagItem *taglist, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data;

	(void)taglist;

	data = AllocVec(sizeof(*data), MEMF_PUBLIC | MEMF_CLEAR);
	if (data == NULL) {
		return AHISF_ERROR;
	}

	data->base = base;
	data->audioctrl = audioctrl;
	data->playinterrupt.is_Node.ln_Type = NT_INTERRUPT;
	data->playinterrupt.is_Node.ln_Name = (char *)LibName;
	data->playinterrupt.is_Data = data;
	data->playinterrupt.is_Code = ahi_interrupt_entry;
	data->softinterrupt.is_Node.ln_Type = NT_INTERRUPT;
	data->softinterrupt.is_Node.ln_Name = (char *)LibName;
	data->softinterrupt.is_Data = data;
	data->softinterrupt.is_Code = ahi_softint_entry;

	audioctrl->ahiac_DriverData = data;
	return AHISF_MIXING | AHISF_TIMING | AHISF_KNOWSTEREO | AHISF_KNOWHIFI;
}

void UAE_AHIsub_FreeAudio(struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data = (struct UAEAudioData *)audioctrl->ahiac_DriverData;

	if (data == NULL) {
		return;
	}

	(void)UAE_AHIsub_Stop(AHISF_PLAY | AHISF_RECORD, audioctrl, base);

	FreeVec(data);
	audioctrl->ahiac_DriverData = NULL;
}

void UAE_AHIsub_Disable(struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data = (struct UAEAudioData *)audioctrl->ahiac_DriverData;

	(void)base;
	if (data != NULL) {
		data->disable_count++;
	}
}

void UAE_AHIsub_Enable(struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data = (struct UAEAudioData *)audioctrl->ahiac_DriverData;

	(void)base;
	if (data != NULL && data->disable_count != 0) {
		data->disable_count--;
	}
}

ULONG UAE_AHIsub_Start(ULONG flags, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data = (struct UAEAudioData *)audioctrl->ahiac_DriverData;
	LONG opened_freq;
	ULONG blocksamples;
	ULONG mixbuffersize;
	ULONG hostblocksize;
	ULONG hostblocksamples;
	ULONG hostbuffersize;
	ULONG mixhostbuffersize;
	BOOL hifi;

	if (data == NULL) {
		return AHIE_UNKNOWN;
	}

	if ((flags & AHISF_RECORD) != 0) {
		return AHIE_UNKNOWN;
	}

	(void)UAE_AHIsub_Stop(flags, audioctrl, base);

	if ((flags & AHISF_PLAY) == 0) {
		return AHIE_OK;
	}

	data->mixfreq = audioctrl->ahiac_MixFreq;
	if (data->mixfreq == 0) {
		data->mixfreq = 44100;
		audioctrl->ahiac_MixFreq = data->mixfreq;
	}

	if (audioctrl->ahiac_BuffSamples == 0) {
		audioctrl->ahiac_BuffSamples = data->mixfreq / 50;
		if (audioctrl->ahiac_BuffSamples == 0) {
			audioctrl->ahiac_BuffSamples = UAE_DEFAULT_BLOCK_SAMPLES;
		}
	}

	hifi = (audioctrl->ahiac_Flags & AHIACF_HIFI) != 0;
	if (audioctrl->ahiac_BuffSize == 0) {
		audioctrl->ahiac_BuffSize = audioctrl->ahiac_BuffSamples * UAE_CHANNELS * (hifi ? sizeof(LONG) : sizeof(WORD));
	}

	blocksamples = audioctrl->ahiac_BuffSamples;
	mixbuffersize = audioctrl->ahiac_BuffSize;
	hostblocksize = blocksamples * UAE_CHANNELS * (UAE_BITS / 8);
	hostblocksamples = blocksamples * UAE_BUFFER_BLOCKS;
	hostbuffersize = hostblocksize * UAE_BUFFER_BLOCKS;
	mixhostbuffersize = mixbuffersize * UAE_BUFFER_BLOCKS;

	data->playbuffers[0] = AllocVec(hostbuffersize, MEMF_PUBLIC | MEMF_CLEAR);
	data->playbuffers[1] = AllocVec(hostbuffersize, MEMF_PUBLIC | MEMF_CLEAR);
	if (hifi) {
		data->mixbuffers[0] = AllocVec(mixhostbuffersize, MEMF_PUBLIC | MEMF_CLEAR);
		data->mixbuffers[1] = AllocVec(mixhostbuffersize, MEMF_PUBLIC | MEMF_CLEAR);
	}
	if (data->playbuffers[0] == NULL || data->playbuffers[1] == NULL || (hifi && (data->mixbuffers[0] == NULL || data->mixbuffers[1] == NULL))) {
		if (data->playbuffers[0] != NULL) {
			FreeVec(data->playbuffers[0]);
			data->playbuffers[0] = NULL;
		}
		if (data->playbuffers[1] != NULL) {
			FreeVec(data->playbuffers[1]);
			data->playbuffers[1] = NULL;
		}
		if (data->mixbuffers[0] != NULL) {
			FreeVec(data->mixbuffers[0]);
			data->mixbuffers[0] = NULL;
		}
		if (data->mixbuffers[1] != NULL) {
			FreeVec(data->mixbuffers[1]);
			data->mixbuffers[1] = NULL;
		}
		return AHIE_NOMEM;
	}
	data->mixbuffsize = mixbuffersize;
	data->hostblocksize = hostblocksize;
	data->hostbuffersize = hostbuffersize;
	data->blocksamples = blocksamples;
	data->hostblocksamples = hostblocksamples;
	data->activebuffer = 0;
	data->softintbuffer = NULL;
	data->disable_count = 0;
	data->softint_active = FALSE;

	opened_freq = uae_ahi_open(base->trap_addr, UAE_UNIT, data->mixfreq, data->hostblocksamples, UAE_CHANNELS, UAE_BITS);
	if (opened_freq == 0) {
		FreeVec(data->playbuffers[0]);
		FreeVec(data->playbuffers[1]);
		if (data->mixbuffers[0] != NULL) {
			FreeVec(data->mixbuffers[0]);
		}
		if (data->mixbuffers[1] != NULL) {
			FreeVec(data->mixbuffers[1]);
		}
		data->playbuffers[0] = NULL;
		data->playbuffers[1] = NULL;
		data->mixbuffers[0] = NULL;
		data->mixbuffers[1] = NULL;
		return AHIE_UNKNOWN;
	}
	data->mixfreq = (ULONG)opened_freq;

	data->running = TRUE;
	AddIntServer(INTB_EXTER, &data->playinterrupt);
	data->interrupt_added = TRUE;

	return AHIE_OK;
}

ULONG UAE_AHIsub_Update(ULONG flags, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)flags;
	(void)audioctrl;
	(void)base;
	return AHIE_OK;
}

ULONG UAE_AHIsub_Stop(ULONG flags, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	struct UAEAudioData *data = (struct UAEAudioData *)audioctrl->ahiac_DriverData;
	ULONG i;

	if (data == NULL) {
		return AHIE_OK;
	}

	if ((flags & AHISF_PLAY) != 0) {
		data->running = FALSE;
		if (data->interrupt_added) {
			RemIntServer(INTB_EXTER, &data->playinterrupt);
			data->interrupt_added = FALSE;
		}

		for (i = 0; i < 50 && data->softint_active; i++) {
			Delay(1);
		}
		data->softint_active = FALSE;
		data->softintbuffer = NULL;
	}

	(void)uae_ahi_close(base->trap_addr, UAE_UNIT);

	if (data->playbuffers[0] != NULL) {
		FreeVec(data->playbuffers[0]);
		data->playbuffers[0] = NULL;
	}
	if (data->playbuffers[1] != NULL) {
		FreeVec(data->playbuffers[1]);
		data->playbuffers[1] = NULL;
	}
	if (data->mixbuffers[0] != NULL) {
		FreeVec(data->mixbuffers[0]);
		data->mixbuffers[0] = NULL;
	}
	if (data->mixbuffers[1] != NULL) {
		FreeVec(data->mixbuffers[1]);
		data->mixbuffers[1] = NULL;
	}

	return AHIE_OK;
}

ULONG UAE_AHIsub_SetVol(UWORD channel, Fixed volume, sposition pan, struct AHIAudioCtrlDrv *audioctrl, ULONG flags, struct UAEBase *base)
{
	(void)channel;
	(void)volume;
	(void)pan;
	(void)audioctrl;
	(void)flags;
	(void)base;
	return AHIS_UNKNOWN;
}

ULONG UAE_AHIsub_SetFreq(UWORD channel, ULONG freq, struct AHIAudioCtrlDrv *audioctrl, ULONG flags, struct UAEBase *base)
{
	(void)channel;
	(void)freq;
	(void)audioctrl;
	(void)flags;
	(void)base;
	return AHIS_UNKNOWN;
}

ULONG UAE_AHIsub_SetSound(UWORD channel, UWORD sound, ULONG offset, LONG length, struct AHIAudioCtrlDrv *audioctrl, ULONG flags, struct UAEBase *base)
{
	(void)channel;
	(void)sound;
	(void)offset;
	(void)length;
	(void)audioctrl;
	(void)flags;
	(void)base;
	return AHIS_UNKNOWN;
}

ULONG UAE_AHIsub_SetEffect(APTR effect, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)effect;
	(void)audioctrl;
	(void)base;
	return AHIS_UNKNOWN;
}

ULONG UAE_AHIsub_LoadSound(UWORD sound, ULONG type, APTR info, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)sound;
	(void)type;
	(void)info;
	(void)audioctrl;
	(void)base;
	return AHIS_UNKNOWN;
}

ULONG UAE_AHIsub_UnloadSound(UWORD sound, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)sound;
	(void)audioctrl;
	(void)base;
	return AHIS_UNKNOWN;
}

LONG UAE_AHIsub_GetAttr(ULONG attribute, LONG argument, LONG def, struct TagItem *taglist, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)taglist;
	(void)base;

	switch (attribute) {
	case AHIDB_Bits:
		return UAE_BITS;
	case AHIDB_Frequencies:
		return FREQUENCY_COUNT;
	case AHIDB_Frequency:
		if (argument < 0) {
			return Frequencies[0];
		}
		if (argument >= FREQUENCY_COUNT) {
			return Frequencies[FREQUENCY_COUNT - 1];
		}
		return Frequencies[argument];
	case AHIDB_Index:
		return closest_frequency_index(argument);
	case AHIDB_MinMixFreq:
		return Frequencies[0];
	case AHIDB_MaxMixFreq:
		return Frequencies[FREQUENCY_COUNT - 1];
	case AHIDB_MaxChannels:
		return UAE_MAX_AHI_CHANNELS;
	case AHIDB_MaxPlaySamples:
		if (audioctrl != NULL && audioctrl->ahiac_BuffSamples != 0) {
			return audioctrl->ahiac_BuffSamples * UAE_BUFFER_BLOCKS;
		}
		return def;
	case AHIDB_Author:
		return (LONG)AuthorString;
	case AHIDB_Copyright:
		return (LONG)CopyrightString;
	case AHIDB_Version:
		return (LONG)LibIDString + 1;
	case AHIDB_Record:
	case AHIDB_FullDuplex:
		return FALSE;
	case AHIDB_Realtime:
		return TRUE;
	case AHIDB_Outputs:
		return 1;
	case AHIDB_Output:
		return (LONG)OutputString;
	default:
		return def;
	}
}

LONG UAE_AHIsub_HardwareControl(ULONG attribute, LONG argument, struct AHIAudioCtrlDrv *audioctrl, struct UAEBase *base)
{
	(void)attribute;
	(void)argument;
	(void)audioctrl;
	(void)base;
	return FALSE;
}
