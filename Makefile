# m68k-amigaos-gcc -Isrc -noixemul -fomit-frame-pointer -Os -std=c99 -o host-run src/host-run.c
all: host-run host-multiview host-shell

VERSION		= 2.0
DATE		= 2026-04-13

CC			= m68k-amigaos-gcc
INCLUDES	= -Isrc
CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra
VERFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\""

host-run: src/host-run.c
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-run.c -o $@

host-multiview: src/host-multiview.c
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-multiview.c -o $@

host-shell: src/host-shell.c
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-shell.c -o $@

debug: CFLAGS += -DDEBUG -g
debug: clean all

clean:
	rm -f host-run host-multiview host-shell
