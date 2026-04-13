#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uae_pragmas.h"

static const char __ver[40] = "$VER: Host-MultiView v1.9 (2025-12-26)";

int print_usage()
{
    printf("Host-MultiView v1.9\n");
    printf("Host-MultiView is a command line tool to open files with the host default handler, from within UAE.\n");
    printf("%s\nUsage: host-multiview <filename> [filename2 ...]\n", __ver);
    return 0;
}

int main(int argc, char *argv[])
{
    BPTR lock;
    char filename[256];

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing filename argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    /* Iterate through all arguments and request the host to view them */
    for (int i = 1; i < argc; i++)
    {
        char *target = argv[i];

        /* Try to resolve as a file path first to get the host path (skip URLs) */
        if (!strstr(argv[i], "://") && (lock = Lock(argv[i], ACCESS_READ)))
        {
            if (NativeDosOp((ULONG)0, (ULONG)lock, (ULONG)filename, (ULONG)sizeof(filename)) == 0) {
                 UnLock(lock);
                 target = filename;
            } else {
                 UnLock(lock);
            }
        }
        
        /* Send the request to Amiberry */
        /* Opcode 94 handles the quoting and OS-specific command (open/xdg-open) */
        HostShell_View((UBYTE *)target);
    }
    
    return 0;
}