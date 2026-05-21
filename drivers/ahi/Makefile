# SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
# SPDX-License-Identifier: GPL-3.0-or-later

VERSION		?= 2.3
DATE		?= 2026-04-30

ifeq ($(origin VASMM68K),default)
VASMM68K	= vasmm68k_mot
endif
VASMM68K_CMD	= $(if $(VASMM68K),$(VASMM68K),vasmm68k_mot)

BUILD_DIR	= build
AHI_PACKAGE_DIR	= package
DRIVER		= $(AHI_PACKAGE_DIR)/Devs/AHI/uae.audio
MODE		= $(AHI_PACKAGE_DIR)/Devs/AudioModes/UAE

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

VASM_INCLUDE_FLAGS = -I$(V1_INCLUDE_DIR) -I/opt/m68k-amigaos/m68k-amigaos/include -I/opt/m68k-amigaos/m68k-amigaos/ndk-include
VASM_HUNK_FLAGS = -m68020 -devpac -Fhunkexe $(VASM_INCLUDE_FLAGS)
VASM_BIN_FLAGS = -m68020 -devpac -Fbin $(VASM_INCLUDE_FLAGS)

.PHONY: all clean

all: $(DRIVER) $(MODE)

$(BUILD_DIR):
	mkdir -p $@

$(AHI_PACKAGE_DIR)/Devs/AHI $(AHI_PACKAGE_DIR)/Devs/AudioModes:
	mkdir -p $@

$(BUILD_DIR)/uae.audio: $(V1_DRIVER) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_HUNK_FLAGS) -o $@ $<

$(DRIVER): $(BUILD_DIR)/uae.audio | $(AHI_PACKAGE_DIR)/Devs/AHI
	cp $< $@

$(BUILD_DIR)/UAE: $(V1_MODE) $(V1_INCLUDE_FILES) | $(BUILD_DIR)
	$(VASMM68K_CMD) $(VASM_BIN_FLAGS) -o $@ $<

$(MODE): $(BUILD_DIR)/UAE | $(AHI_PACKAGE_DIR)/Devs/AudioModes
	cp $< $@

clean:
	rm -rf $(BUILD_DIR) $(AHI_PACKAGE_DIR)
