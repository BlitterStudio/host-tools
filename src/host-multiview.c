#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uae_pragmas.h"

static const char version[] = "$VER: Host-MultiView v" VERSION_STR " (" DATE_STR ")";

int print_usage()
{
    printf("Host-MultiView v%s\n", VERSION_STR);
    printf("Host-MultiView is a command line tool to open files or URLs with the host default handler, from within UAE.\n");
    printf("%s\nUsage: host-multiview <filename|URL> [filename2|URL2 ...]\n", version);
    return 0;
}

int main(int argc, char *argv[])
{
    BPTR lock;
    char filename[1024];

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing filename or URL argument\n");
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
        if (!strstr(argv[i], "://") && ((lock = Lock(argv[i], ACCESS_READ))))
        {
            if (NativeDosOp(0, (ULONG)lock, (ULONG)filename, sizeof(filename)) == 0) {
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