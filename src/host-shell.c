#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "uae_pragmas.h"

#define OUTBUFSIZE 4095

static const char version[] = "$VER: Host-Shell v" VERSION_STR " (" DATE_STR ")";
char outbuf[OUTBUFSIZE + 1];

int main(int argc, char *argv[])
{
    char command[1024] = "";
    char buffer[4096];
    BPTR in, out;
    long handle;
    BOOL brk = FALSE;
    long actual;

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc == 2 && strcmp(argv[1], "?") == 0)
    {
        printf("Host-Shell v%s\n", VERSION_STR);
        printf("Host-Shell opens an interactive terminal session on the host system.\n");
        printf("%s\nUsage: host-shell [command]\n", version);
        return 0;
    }

    // Combine arguments into command string
    for (int i = 1; i < argc; i++)
    {
        if (i > 1) strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    if ((DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 0)))
    {
        in = Input();
        out = Output();

        // Enable Raw Mode
        SetMode(in, 1);

        handle = HostShell_Open((UBYTE *)command);
        if (handle == 0) {
            printf("Failed to open host shell session.\n");
            SetMode(in, 0);
            CloseLibrary((struct Library *)DOSBase);
            return 10;
        }

        BOOL esc_pending = FALSE;

        while (!brk)
        {
            // Check for output from host
            actual = HostShell_Read(handle, (UBYTE *)buffer, sizeof(buffer) - 2);
            if (actual > 0)
            {
                int outptr = 0;
                for (int i = 0; i < actual; i++) {
                    unsigned char c = (unsigned char)buffer[i];
                    if (esc_pending) {
                        if (c == 0x5B) { // '['
                            outbuf[outptr++] = 0x9B; // CSI
                        } else {
                            outbuf[outptr++] = 0x1B; // Original ESC
                            outbuf[outptr++] = c;
                        }
                        esc_pending = FALSE;
                    } else {
                        if (c == 0x1B) {
                            esc_pending = TRUE;
                        } else {
                            outbuf[outptr++] = c;
                        }
                    }
                    
                    // Safety check for outbuf overflow (should rarely happen given the math)
                    if (outptr >= OUTBUFSIZE) {
                        Write(out, outbuf, outptr);
                        outptr = 0;
                    }
                }
                if (outptr > 0) {
                    Write(out, outbuf, outptr);
                }
            }
            else if (actual < 0) // Error or closed
            {
                break;
            }

            // Check for input from Amiga user
            // Wait up to 20ms (20000 microseconds)
            if (WaitForChar(in, 20000)) 
            {
                // Read less than buffer size to allow for expansion (max 2x)
                actual = Read(in, buffer, 1024);
                if (actual > 0)
                {
                    int outptr = 0;
                    for (int i = 0; i < actual; i++) {
                        unsigned char c = (unsigned char)buffer[i];
                        if (c == 0x9B) { // Amiga CSI
                            // Convert to ANSI ESC [
                            outbuf[outptr++] = 0x1B;
                            outbuf[outptr++] = 0x5B;
                        } else if (c == 0x08) { // Backspace
                            // Convert BS (0x08) to DEL (0x7F)
                            outbuf[outptr++] = 0x7F;
                        } else {
                            outbuf[outptr++] = c;
                        }
                    }
                    if (outptr > 0) {
                        HostShell_Write(handle, (UBYTE *)outbuf, outptr);
                    }
                }
                else if (actual == 0) // EOF
                {
                    // Maybe break? Or just ignore?
                }
            }

            if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
            {
                SetSignal(0, SIGBREAKF_CTRL_C); // Clear the signal
                // Send Ctrl-C (ETX) to host
                char ctrlc = 0x03;
                HostShell_Write(handle, (UBYTE *)&ctrlc, 1);
            }
        }

        HostShell_Close(handle);
        SetMode(in, 0); // Restore Cooked Mode
        CloseLibrary((struct Library *)DOSBase);
    }
    return 0;
}