# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

TOOLS		= host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info
TEST_BINS	= tests/test_host_common.out tests/test_host_command_builders.out tests/test_host_edit_command.out
TEST_SCRIPTS	= tests/test_package_layout.sh tests/test_ahi_driver_source.sh
TESTS		= $(TEST_BINS) $(TEST_SCRIPTS)
COMMON_HEADERS	= src/host_common.h src/host_path.h src/host_capture.h src/host_clip_command.h src/host_edit_command.h src/host_info_command.h src/host_notify_command.h src/host_reveal_command.h src/uae_pragmas.h
PACKAGE		= Host-Tools-$(VERSION).lha
PACKAGE_ROOT	= Host-Tools
PACKAGE_DIR	?= build/package
PACKAGE_STAGE	= $(PACKAGE_DIR)/$(PACKAGE_ROOT)
AHI_AUDIO	= drivers/ahi/package/Devs/AHI/uae.audio
AHI_MODE	= drivers/ahi/package/Devs/AudioModes/UAE
AHI_FILES	= $(AHI_AUDIO) $(AHI_MODE)
AHI_SOURCES	= drivers/ahi/Makefile \
	drivers/ahi/src/v1/uae.audio.asm \
	drivers/ahi/src/v1/UAE.asm \
	drivers/ahi/src/v1/include/hardware/all.i \
	drivers/ahi/src/v1/include/lvos/exec_lib.i \
	drivers/ahi/src/v1/include/lvos/utility_lib.i \
	drivers/ahi/src/v1/include/lvos/dos_lib.i \
	drivers/ahi/src/v1/include/lvos/cardres_lib.i \
	drivers/ahi/src/v1/include/lvos/ahi_sub_lib.i \
	drivers/ahi/src/v1/include/macros.i
HELP_GUIDE	= package/Help/Host-Tools.guide
DRAWER_ICON	= package/icons/drawer.info
HELP_ICON	= package/icons/Help.info
INSTALL_ICON	= package/icons/Install.info
README_ICON	= package/icons/readme.info
GUIDE_ICON	= package/icons/guide.info

.SUFFIXES:
.PHONY: all test debug package package-dir ahi clean

all: $(TOOLS) $(AHI_FILES)
test: $(TESTS)
	@for test in $(TESTS); do \
		case "$$test" in \
			*.sh) sh "$$test" ;; \
			*) ./$$test ;; \
		esac || exit $$?; \
	done

VERSION		= 2.3
DATE		= 2026-04-30

ifeq ($(origin CC),default)
CC			= m68k-amigaos-gcc
endif
INCLUDES	= -Isrc
CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra -Wstrict-prototypes
VERFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\""
HOST_CC		?= $(shell command -v x86_64-linux-gnu-gcc 2>/dev/null || command -v cc 2>/dev/null || printf cc)
HOST_NATIVE_FLAGS = $(if $(findstring x86_64-linux-gnu-gcc,$(notdir $(HOST_CC))),-B/usr/bin/x86_64-linux-gnu- -fuse-ld=bfd,)
HOST_CFLAGS	= -std=c99 -Wall -Wextra -Wstrict-prototypes -Isrc

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

tests/test_host_common.out: tests/test_host_common.c src/host_common.h
	$(HOST_CC) $(HOST_NATIVE_FLAGS) $(HOST_CFLAGS) tests/test_host_common.c -o $@

tests/test_host_command_builders.out: tests/test_host_command_builders.c src/host_clip_command.h src/host_common.h src/host_info_command.h src/host_notify_command.h src/host_reveal_command.h
	$(HOST_CC) $(HOST_NATIVE_FLAGS) $(HOST_CFLAGS) tests/test_host_command_builders.c -o $@

tests/test_host_edit_command.out: tests/test_host_edit_command.c src/host_edit_command.h src/host_common.h
	$(HOST_CC) $(HOST_NATIVE_FLAGS) $(HOST_CFLAGS) tests/test_host_edit_command.c -o $@

debug: CFLAGS += -DDEBUG -g
debug: clean all

ahi: $(AHI_FILES)

$(AHI_FILES): $(AHI_SOURCES)
	$(MAKE) -C drivers/ahi VERSION=$(VERSION) DATE=$(DATE)

package-dir: all package/Install $(HELP_GUIDE) $(DRAWER_ICON) $(HELP_ICON) $(INSTALL_ICON) $(README_ICON) $(GUIDE_ICON)
	rm -rf $(PACKAGE_STAGE) $(PACKAGE_DIR)/$(PACKAGE_ROOT).info
	mkdir -p $(PACKAGE_STAGE)/C
	mkdir -p $(PACKAGE_STAGE)/Help
	cp $(TOOLS) $(PACKAGE_STAGE)/C/
	cp package/Install $(PACKAGE_STAGE)/Install
	cp $(INSTALL_ICON) $(PACKAGE_STAGE)/Install.info
	cp README.md $(PACKAGE_STAGE)/README
	cp $(README_ICON) $(PACKAGE_STAGE)/README.info
	cp $(HELP_GUIDE) $(PACKAGE_STAGE)/Help/Host-Tools.guide
	cp $(GUIDE_ICON) $(PACKAGE_STAGE)/Help/Host-Tools.guide.info
	cp $(HELP_ICON) $(PACKAGE_STAGE)/Help.info
	cp $(DRAWER_ICON) $(PACKAGE_DIR)/$(PACKAGE_ROOT).info
	if [ -f $(AHI_AUDIO) ] && [ -f $(AHI_MODE) ]; then \
		mkdir -p $(PACKAGE_STAGE)/Devs/AHI $(PACKAGE_STAGE)/Devs/AudioModes; \
		cp $(AHI_AUDIO) $(PACKAGE_STAGE)/Devs/AHI/uae.audio; \
		cp $(AHI_MODE) $(PACKAGE_STAGE)/Devs/AudioModes/UAE; \
	fi

package: package-dir
	rm -f $(PACKAGE)
	cd $(PACKAGE_DIR) && lha a $(CURDIR)/$(PACKAGE) $(PACKAGE_ROOT) $(PACKAGE_ROOT).info

clean:
	rm -f $(TOOLS) $(TEST_BINS)
	$(MAKE) -C drivers/ahi clean
