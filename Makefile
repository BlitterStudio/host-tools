# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

# m68k-amigaos-gcc -Isrc -noixemul -fomit-frame-pointer -Os -std=c99 -o host-run src/host-run.c
all: host-run host-multiview host-shell

VERSION		= 2.0
DATE		= 2026-04-13

ifeq ($(origin CC),default)
CC			= m68k-amigaos-gcc
endif
INCLUDES	= -Isrc
CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra
VERFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\""

host-run: src/host-run.c src/host_common.h src/uae_pragmas.h
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-run.c -o $@

host-multiview: src/host-multiview.c src/host_common.h src/uae_pragmas.h
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-multiview.c -o $@

host-shell: src/host-shell.c src/host_common.h src/uae_pragmas.h
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-shell.c -o $@

debug: CFLAGS += -DDEBUG -g
debug: clean all

clean:
	rm -f host-run host-multiview host-shell
