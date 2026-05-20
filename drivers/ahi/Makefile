# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

VERSION		?= 2.3
DATE		?= 2026-04-30

ifeq ($(origin CC),default)
CC		= m68k-amigaos-gcc
endif
ifeq ($(origin OBJCOPY),default)
OBJCOPY		= m68k-amigaos-objcopy
endif
ifeq ($(origin STRIP),default)
STRIP		= m68k-amigaos-strip
endif
STRIP_CMD	= $(if $(STRIP),$(STRIP),m68k-amigaos-strip)
OBJCOPY_CMD	= $(if $(OBJCOPY),$(OBJCOPY),m68k-amigaos-objcopy)

BUILD_DIR	= build
AHI_PACKAGE_DIR	= package
DRIVER		= $(AHI_PACKAGE_DIR)/Devs/AHI/uae.audio
MODE		= $(AHI_PACKAGE_DIR)/Devs/AudioModes/UAE

SOURCES		= src/uae.audio.c src/uae-gates.S
OBJECTS		= $(BUILD_DIR)/uae.audio.o $(BUILD_DIR)/uae-gates.o

CFLAGS		= -mcpu=68020 -noixemul -Os -fomit-frame-pointer -std=c99 -Wall -Wextra -Wstrict-prototypes
CPPFLAGS	= -DVERSION_STR="\"$(VERSION)\"" -DDATE_STR="\"$(DATE)\""
ASFLAGS		= -mcpu=68020 -noixemul
LDFLAGS		= -mcpu=68020 -noixemul -nostartfiles

.PHONY: all clean

all: $(DRIVER) $(MODE)

$(BUILD_DIR):
	mkdir -p $@

$(AHI_PACKAGE_DIR)/Devs/AHI $(AHI_PACKAGE_DIR)/Devs/AudioModes:
	mkdir -p $@

$(BUILD_DIR)/uae.audio.o: src/uae.audio.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/uae-gates.o: src/uae-gates.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) $(CPPFLAGS) -c $< -o $@

$(BUILD_DIR)/uae.audio: $(OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@
	$(STRIP_CMD) $@

$(DRIVER): $(BUILD_DIR)/uae.audio | $(AHI_PACKAGE_DIR)/Devs/AHI
	cp $< $@

$(BUILD_DIR)/UAE.o: src/UAE.S | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/UAE: $(BUILD_DIR)/UAE.o
	$(OBJCOPY_CMD) --output-target=binary $< $@

$(MODE): $(BUILD_DIR)/UAE | $(AHI_PACKAGE_DIR)/Devs/AudioModes
	cp $< $@

clean:
	rm -rf $(BUILD_DIR) $(AHI_PACKAGE_DIR)
