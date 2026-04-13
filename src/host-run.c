#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uae_pragmas.h"

static const char __ver[40] = "$VER: Host-Run v1.9 (2025-12-26)";

#define MAX_CMD_LEN 4096

int print_usage()
{
    printf("Host-Run v1.9\n");
    printf("Host-Run is a command line tool to run host commands from within UAE.\n");
    printf("%s\nUsage: host-run <command> <argument1> <argument2> ...\n", __ver);
    return 0;
}

// Check if a character is safe for shell (no quoting needed)
int is_safe_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '.' || c == '_' || c == '-' || c == '/' || c == ':' || c == '+' || c == '=' || c == ',' || c == '@';
}

// Append a string to the buffer, quoting it if necessary
void append_quoted(char *dest, const char *src, size_t max_len) {
    size_t current_len = strlen(dest);
    if (current_len >= max_len - 1) return;

    int needs_quote = 0;
    for (const char *p = src; *p; p++) {
        if (!is_safe_char(*p)) {
            needs_quote = 1;
            break;
        }
    }

    // Ensure we have space for opening quote
    if (needs_quote) {
        if (current_len < max_len - 1) dest[current_len++] = '\'';
        dest[current_len] = '\0';
    }

    for (const char *p = src; *p; p++) {
        if (current_len >= max_len - 4) break; // Reserve space for close quote + null

        if (*p == '\'') {
            // Escape single quote: ' -> '\''
            strcat(dest, "'\\''");
            current_len += 4;
        } else {
            dest[current_len++] = *p;
            dest[current_len] = '\0';
        }
    }

    // Closing quote
    if (needs_quote) {
        if (current_len < max_len - 1) dest[current_len++] = '\'';
        dest[current_len] = '\0';
    }
}

int main(int argc, char *argv[])
{
    BPTR lock;
    char command[MAX_CMD_LEN] = "";
    char filename[256];

    if (!InitUAEResource())
    {
        printf("UAEResource not found!\n");
        return 2;
    }

    if (argc <= 1)
    {
        printf("Missing argument\n");
        return print_usage();
    }

    if (strcmp(argv[1], "?") == 0)
    {
        return print_usage();
    }

    for (int i = 1; i < argc; i++)
    {
        // Try to resolve as a file path first (skip URLs to avoid volume requester)
        int is_resolved_file = 0;
        if (!strstr(argv[i], "://") && (lock = Lock(argv[i], ACCESS_READ)))
        {
            if (NativeDosOp((ULONG)0, (ULONG)lock, (ULONG)filename, (ULONG)sizeof(filename)) == 0) {
                 UnLock(lock);
                 is_resolved_file = 1;
            } else {
                 UnLock(lock);
            }
        }

        // Separate arguments with space
        if (i > 1) {
            strncat(command, " ", MAX_CMD_LEN - strlen(command) - 1);
        }

        if (is_resolved_file) {
            append_quoted(command, filename, MAX_CMD_LEN);
        } else {
            append_quoted(command, argv[i], MAX_CMD_LEN);
        }
    }

#ifdef DEBUG
    printf("DEBUG: argc=%d, command=%s\n", argc, command);
#endif
    ExecuteOnHost((UBYTE *)&command);
    return 0;
}