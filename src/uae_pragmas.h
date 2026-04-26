/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UAE_PRAGMAS_H
#define UAE_PRAGMAS_H

#include <proto/exec.h>
#include <proto/dos.h>

#if defined(__GNUC__)
#define UAE_UNUSED __attribute__((unused))
#else
#define UAE_UNUSED
#endif

/*
 * Configuration structure
 */
struct UAE_CONFIG
{
       ULONG             version;
       ULONG             chipmemsize;
       ULONG             slowmemsize;
       ULONG             fastmemsize;
       ULONG             framerate;
       ULONG             do_output_sound;
       ULONG             do_fake_joystick;
       ULONG             keyboard;
       UBYTE             disk_in_df0;
       UBYTE             disk_in_df1;
       UBYTE             disk_in_df2;
       UBYTE             disk_in_df3;
       char              df0_name[256];
       char              df1_name[256];
       char              df2_name[256];
       char              df3_name[256];
};

struct uaebase
{
	struct Library uae_lib;
	UWORD uae_version;
	UWORD uae_revision;
	UWORD uae_subrevision;
	UWORD zero;
	APTR uae_rombase;
};

static struct uaebase *UAEResource;

static int (*calltrap)(int arg0, ...) = (int (*)(int arg0, ...))0xF0FF60;

static int InitUAEResource(void)
{
	UAEResource = (struct uaebase *)OpenResource((UBYTE *)"uae.resource");
	if (UAEResource)
	{
		calltrap = (int (*)(int arg0, ...))((BYTE *)UAEResource->uae_rombase + 0xFF60);
		return 1;
	}
	return 0;
}

static int UAE_UNUSED GetVersion(void)
{
    return calltrap (0);
}
static int UAE_UNUSED GetUaeConfig(struct UAE_CONFIG *a)
{
    return calltrap (1, a);
}
static int UAE_UNUSED SetUaeConfig(struct UAE_CONFIG *a)
{
    return calltrap (2, a);
}
static int UAE_UNUSED HardReset(void)
{
    return calltrap (3);
}
static int UAE_UNUSED Reset(void)
{
    return calltrap (4);
}
static int UAE_UNUSED EjectDisk(ULONG drive)
{
    return calltrap (5, "", drive);
}
static int UAE_UNUSED InsertDisk(UBYTE *name, ULONG drive)
{
    return calltrap (5, name, drive);
}
static int UAE_UNUSED EnableSound(void)
{
    return calltrap (6, 2);
}
static int UAE_UNUSED DisableSound(void)
{
    return calltrap (6, 1);
}
static int UAE_UNUSED EnableJoystick(void)
{
    return calltrap (7, 1);
}
static int UAE_UNUSED DisableJoystick(void)
{
    return calltrap (7, 0);
}
static int UAE_UNUSED SetFrameRate(ULONG rate)
{
    return calltrap (8, rate);
}
static int UAE_UNUSED ChgCMemSize(ULONG mem)
{
    return calltrap (9, mem);
}
static int UAE_UNUSED ChgSMemSize(ULONG mem)
{
    return calltrap (10, mem);
}
static int UAE_UNUSED ChgFMemSize(ULONG mem)
{
    return calltrap (11, mem);
}
static int UAE_UNUSED ChangeLanguage(ULONG lang)
{
    return calltrap (12, lang);
}
static int UAE_UNUSED ExitEmu(void)
{
    return calltrap (13);
}
static int UAE_UNUSED GetDisk(ULONG drive, UBYTE *name)
{
    return calltrap (14, drive, name);
}
static int UAE_UNUSED DebugFunc(void)
{
    return calltrap (15);
}
static int UAE_UNUSED Minimize(void)
{
    return calltrap(68);
}
static int UAE_UNUSED ExecuteNativeCode()
{
    return calltrap(69);
}
static int UAE_UNUSED UnprotectMapRom()
{
    return calltrap(80);
}
static int UAE_UNUSED EmuConfig(int mode, UBYTE *name, ULONG dst, ULONG maxlength)
{
    return calltrap(81, mode, name, dst, maxlength);
}
static int UAE_UNUSED EmuConfigModify(int mode, UBYTE *parms, ULONG size, ULONG out, ULONG outsize)
{
    return calltrap(82, mode, parms, size, out, outsize);
}
static int UAE_UNUSED IsMMKeyboard()
{
    return calltrap(83);
}
static int UAE_UNUSED NativeDosOp(ULONG mode, ULONG lock, ULONG out, ULONG outsize)
{
    return calltrap(85, mode, lock, out, outsize);
}
static int UAE_UNUSED GetCpuRate()
{
    return calltrap(87);
}
static int UAE_UNUSED ExecuteOnHost(UBYTE *name)
{
    return calltrap (88, name);
}

static int UAE_UNUSED HostShell_Open(UBYTE *command)
{
    return calltrap(90, command);
}
static int UAE_UNUSED HostShell_Read(ULONG handle, UBYTE *buffer, ULONG size)
{
    return calltrap(91, handle, buffer, size);
}
static int UAE_UNUSED HostShell_Write(ULONG handle, UBYTE *buffer, ULONG size)
{
    return calltrap(92, handle, buffer, size);
}
static int UAE_UNUSED HostShell_Close(ULONG handle)
{
    return calltrap(93, handle);
}
static ULONG UAE_UNUSED HostShell_Status(ULONG handle)
{
    return (ULONG)calltrap(94, handle);
}

static int UAE_UNUSED HostShell_View(UBYTE *filename)
{
    return calltrap(89, filename);
}

#endif
