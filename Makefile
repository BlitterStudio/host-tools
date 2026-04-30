# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

TOOLS		= host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info
TESTS		= tests/test_host_edit_command.out
COMMON_HEADERS	= src/host_common.h src/host_path.h src/host_capture.h src/host_edit_command.h src/uae_pragmas.h
PACKAGE		= Host-Tools-$(VERSION).lha

all: $(TOOLS)
test: $(TESTS)
	./$(TESTS)

VERSION		= 2.2
DATE		= 2026-04-30

ifeq ($(origin CC),default)
CC			= m68k-amigaos-gcc
endif
INCLUDES	= -Isrc
CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra
VERFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\""
HOST_CC		?= $(shell command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v cc 2>/dev/null || printf cc)
HOST_NATIVE_FLAGS = $(if $(findstring x86_64-linux-gnu-gcc,$(notdir $(HOST_CC))),-B/usr/bin/x86_64-linux-gnu- -fuse-ld=bfd,)
HOST_CFLAGS	= -std=c99 -Wall -Wextra -Isrc

host-run: src/host-run.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-run.c -o $@

host-multiview: src/host-multiview.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-multiview.c -o $@

host-shell: src/host-shell.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-shell.c -o $@

host-path: src/host-path.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-path.c -o $@

host-reveal: src/host-reveal.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-reveal.c -o $@

host-notify: src/host-notify.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-notify.c -o $@

host-edit: src/host-edit.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-edit.c -o $@

host-clip: src/host-clip.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-clip.c -o $@

host-info: src/host-info.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) $(VERFLAGS) $(INCLUDES) src/host-info.c -o $@

tests/test_host_edit_command.out: tests/test_host_edit_command.c src/host_edit_command.h src/host_common.h
	$(HOST_CC) $(HOST_NATIVE_FLAGS) $(HOST_CFLAGS) tests/test_host_edit_command.c -o $@

debug: CFLAGS += -DDEBUG -g
debug: clean all

package: all
	rm -f $(PACKAGE)
	lha a $(PACKAGE) $(TOOLS) README.md

clean:
	rm -f $(TOOLS) $(TESTS)
