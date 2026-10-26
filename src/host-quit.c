/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * host-quit closes Amiberry. Amiberry's WHDLoad booter runs it on every
 * machine it boots, Kickstart 1.3 and 68000 included, so it is built
 * without the C runtime (libnix needs utility.library on a 68000, which
 * Kickstart 1.3 lacks): its own entry point, AmigaDOS arguments from the
 * registers, and output through dos.library calls that Kickstart 1.2 has.
 */

/* The entry point must be the first code in the executable
 * (-nostartfiles; -fno-toplevel-reorder keeps definition order), so it
 * comes before any header that could emit code. AmigaDOS passes the
 * argument line in A0 and its length in D0. */
struct ExecBase;
struct ExecBase *SysBase;
static int run(const char *args, long length);

int start(register const char *args __asm("a0"), register long length __asm("d0"))
{
    SysBase = *(struct ExecBase **)4;
    return run(args, length);
}

#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "host_common.h"

struct DosLibrary *DOSBase;

#include "uae_pragmas.h"

static const char version[] = "$VER: Host-Quit " VERSION_STR " (" DATE_STR ")";

static long text_length(const char *text)
{
    long length = 0;
    while (text[length])
        length++;
    return length;
}

static void print(const char *text)
{
    BPTR out = Output();
    if (out)
        Write(out, (APTR)text, text_length(text));
}

static int print_usage(void)
{
    print("Host-Quit v" VERSION_STR "\n");
    print("Host-Quit closes Amiberry, as quitting from its menu does.\n");
    print(version + 6);
    print("\nUsage: host-quit\n");
    return 0;
}

static int run(const char *args, long length)
{
    long first = 0;
    long last = length;
    int result;

    DOSBase = (struct DosLibrary *)OpenLibrary((CONST_STRPTR)"dos.library", 0);
    if (!DOSBase)
        return RETURN_FAIL;

    if (!InitUAEResource())
    {
        print("UAEResource not found!\n");
        CloseLibrary((struct Library *)DOSBase);
        return 2;
    }

    /* The argument line ends with a newline; trim blanks around it. */
    while (first < last && (args[first] == ' ' || args[first] == '\t'))
        first++;
    while (last > first && (args[last - 1] == '\n' || args[last - 1] == ' ' || args[last - 1] == '\t'))
        last--;

    if (last - first == 1 && args[first] == '?')
    {
        result = print_usage();
    }
    else if (last > first)
    {
        print("Unexpected argument\n");
        print_usage();
        result = HOST_RETURN_ERROR;
    }
    else
    {
        ExitEmu();
        /* Amiberry closes once the current frame is done. Wait here so a
         * script does not carry on meanwhile; Ctrl-C returns. */
        Wait(SIGBREAKF_CTRL_C);
        result = 0;
    }

    CloseLibrary((struct Library *)DOSBase);
    return result;
}
