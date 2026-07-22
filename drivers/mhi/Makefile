# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

include ../../version.mk
VERSION_WORDS	= $(subst ., ,$(VERSION))
LIB_VERSION	= $(word 1,$(VERSION_WORDS))
LIB_REVISION	= $(word 2,$(VERSION_WORDS))
LIB_REVISION	:= $(if $(LIB_REVISION),$(LIB_REVISION),0)

ifeq ($(origin CC),default)
CC		= m68k-amigaos-gcc
endif

BUILD_DIR	= build
MHI_PACKAGE_DIR = package
LIB_OUT		= $(MHI_PACKAGE_DIR)/Libs/MHI/mhiuae.library
SOURCES		= src/mhiuae_startup.c src/mhiuae.c
HEADERS		= src/mhi_abi.h src/mhiuae.h ../../src/uae_pragmas.h
CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra -Wstrict-prototypes -I../../src
VERFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\"" -DLIB_VERSION=$(LIB_VERSION) -DLIB_REVISION=$(LIB_REVISION)
LDFLAGS		= -nostartfiles

.SUFFIXES:
.PHONY: all clean

all: $(LIB_OUT)

$(BUILD_DIR)/mhiuae.library: $(SOURCES) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(VERFLAGS) $(LDFLAGS) $(SOURCES) -o $@

$(LIB_OUT): $(BUILD_DIR)/mhiuae.library | $(MHI_PACKAGE_DIR)/Libs/MHI
	cp $< $@

$(BUILD_DIR):
	mkdir -p $@

$(MHI_PACKAGE_DIR)/Libs/MHI:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(MHI_PACKAGE_DIR)
