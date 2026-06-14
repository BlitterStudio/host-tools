# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

VERSION		?= 2.4
DATE		?= 2026-06-10

VASMM68K_FALLBACK := $(shell command -v vasmm68k_mot 2>/dev/null || { test -x /opt/m68k-amigaos/bin/vasmm68k_mot && printf /opt/m68k-amigaos/bin/vasmm68k_mot; } || printf vasmm68k_mot)
ifeq ($(origin VASMM68K),undefined)
VASMM68K	:= $(VASMM68K_FALLBACK)
endif
VASMM68K_CMD	= $(if $(strip $(VASMM68K)),$(VASMM68K),$(VASMM68K_FALLBACK))

BUILD_DIR	= build
AHI_PACKAGE_DIR	= package
DRIVER		= $(AHI_PACKAGE_DIR)/Devs/AHI/uae.audio
MODE		= $(AHI_PACKAGE_DIR)/Devs/AudioModes/UAE
V2_AHI_PACKAGE_DIR = package-v2
V2_DRIVER_OUT	= $(V2_AHI_PACKAGE_DIR)/Devs/AHI/uaesnd.audio
V2_MODE_OUT	= $(V2_AHI_PACKAGE_DIR)/Devs/AudioModes/UAESND

V1_SOURCE_DIR	= src/v1
V1_DRIVER	= $(V1_SOURCE_DIR)/uae.audio.asm
V1_MODE		= $(V1_SOURCE_DIR)/UAE.asm
V1_INCLUDE_DIR	= $(V1_SOURCE_DIR)/include
V1_INCLUDE_FILES = \
	$(V1_INCLUDE_DIR)/hardware/all.i \
	$(V1_INCLUDE_DIR)/lvos/exec_lib.i \
	$(V1_INCLUDE_DIR)/lvos/utility_lib.i \
	$(V1_INCLUDE_DIR)/lvos/dos_lib.i \
	$(V1_INCLUDE_DIR)/lvos/cardres_lib.i \
	$(V1_INCLUDE_DIR)/lvos/ahi_sub_lib.i \
	$(V1_INCLUDE_DIR)/macros.i
V2_SOURCE_DIR	= src/v2
V2_DRIVER	= $(V2_SOURCE_DIR)/uaesnd.audio.asm
V2_MODE		= $(V2_SOURCE_DIR)/UAESND.asm

VASM_INCLUDE_FLAGS = -I$(V1_INCLUDE_DIR) -I/opt/m68k-amigaos/m68k-amigaos/include -I/opt/m68k-amigaos/m68k-amigaos/ndk-include
VASM_HUNK_FLAGS = -m68020 -devpac -Fhunkexe $(VASM_INCLUDE_FLAGS)
VASM_BIN_FLAGS = -m68020 -devpac -Fbin $(VASM_INCLUDE_FLAGS)

.SUFFIXES:
.PHONY: all ahi-v2 clean

all: $(DRIVER) $(MODE)
ahi-v2: $(V2_DRIVER_OUT) $(V2_MODE_OUT)

$(BUILD_DIR):
	mkdir -p $@

$(AHI_PACKAGE_DIR)/Devs/AHI $(AHI_PACKAGE_DIR)/Devs/AudioModes:
	mkdir -p $@

$(V2_AHI_PACKAGE_DIR)/Devs/AHI $(V2_AHI_PACKAGE_DIR)/Devs/AudioModes:
	mkdir -p $@

$(BUILD_DIR)/uae.audio: $(V1_DRIVER) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_HUNK_FLAGS) -o $@ $<

$(DRIVER): $(BUILD_DIR)/uae.audio | $(AHI_PACKAGE_DIR)/Devs/AHI
	cp $< $@

$(BUILD_DIR)/UAE: $(V1_MODE) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_BIN_FLAGS) -o $@ $<

$(MODE): $(BUILD_DIR)/UAE | $(AHI_PACKAGE_DIR)/Devs/AudioModes
	cp $< $@

$(BUILD_DIR)/uaesnd.audio: $(V2_DRIVER) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_HUNK_FLAGS) -o $@ $<

$(V2_DRIVER_OUT): $(BUILD_DIR)/uaesnd.audio | $(V2_AHI_PACKAGE_DIR)/Devs/AHI
	cp $< $@

$(BUILD_DIR)/UAESND: $(V2_MODE) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_BIN_FLAGS) -o $@ $<

$(V2_MODE_OUT): $(BUILD_DIR)/UAESND | $(V2_AHI_PACKAGE_DIR)/Devs/AudioModes
	cp $< $@

clean:
	rm -rf $(BUILD_DIR) $(AHI_PACKAGE_DIR) $(V2_AHI_PACKAGE_DIR)
