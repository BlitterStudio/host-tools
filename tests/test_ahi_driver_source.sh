#!/bin/sh
set -eu

MAKEFILE="drivers/ahi/Makefile"
ROOT_MAKEFILE="Makefile"
V1_SOURCE_DIR="drivers/ahi/src/v1"
DRIVER_SOURCE="$V1_SOURCE_DIR/uae.audio.asm"
MODE_SOURCE="$V1_SOURCE_DIR/UAE.asm"
COMPAT_INCLUDE="$V1_SOURCE_DIR/include"

test -f "$MAKEFILE"
test -f "$ROOT_MAKEFILE"
test -d "$V1_SOURCE_DIR"
test -f "$DRIVER_SOURCE"
test -f "$MODE_SOURCE"
test -f "$COMPAT_INCLUDE/hardware/all.i"
test -f "$COMPAT_INCLUDE/lvos/ahi_sub_lib.i"
test -f "$COMPAT_INCLUDE/macros.i"

grep -q 'VASMM68K' "$MAKEFILE"
grep -q 'V1_SOURCE_DIR[[:space:]]*= src/v1' "$MAKEFILE"
grep -q 'V1_DRIVER[[:space:]]*= $(V1_SOURCE_DIR)/uae.audio.asm' "$MAKEFILE"
grep -q 'V1_MODE[[:space:]]*= $(V1_SOURCE_DIR)/UAE.asm' "$MAKEFILE"
grep -q -- '-devpac' "$MAKEFILE"
grep -q -- '-Fhunkexe' "$MAKEFILE"
grep -q -- '-Fbin' "$MAKEFILE"
grep -q 'AHI_SOURCES' "$ROOT_MAKEFILE"
grep -q 'drivers/ahi/src/v1/UAE.asm' "$ROOT_MAKEFILE"

if test -e "drivers/ahi/src/uae.audio.c" || test -e "drivers/ahi/src/uae-gates.S" || test -e "drivers/ahi/src/original"; then
	echo "AHI v1 must contain only the original ASM source layout; C belongs in the future v2 phase" >&2
	exit 1
fi

if grep -q 'src/uae.audio.c\|src/uae-gates.S\|uae.audio.o\|uae-gates.o\|-nostartfiles\|src/original' "$MAKEFILE"; then
	echo "AHI package must build the known-good original ASM driver, not the experimental C port" >&2
	exit 1
fi

grep -F -q 'MINBUFFLEN EQU 2' "$DRIVER_SOURCE"
grep -F -q 'VERSION   EQU 4' "$DRIVER_SOURCE"
grep -F -q 'REVISION  EQU 2' "$DRIVER_SOURCE"
grep -F -q 'Dc.b  "uae 4.4"' "$DRIVER_SOURCE"
grep -F -q 'Dc.b "ahi_winuae",0' "$DRIVER_SOURCE"
grep -F -q 'move.l #$f0ffc0,calladdr' "$DRIVER_SOURCE"
grep -F -q 'moveq #0,d0' "$DRIVER_SOURCE"
grep -F -q 'moveq #2,d0' "$DRIVER_SOURCE"
grep -F -q 'moveq #4,d0' "$DRIVER_SOURCE"
grep -F -q 'AHISF_KNOWHIFI|AHISF_KNOWSTEREO|AHISF_CANRECORD|AHISF_MIXING|AHISF_TIMING' "$DRIVER_SOURCE"
grep -F -q 'move.w (a1),(a0)+' "$DRIVER_SOURCE"
grep -F -q 'move.w 4(a1),(a0)+' "$DRIVER_SOURCE"
grep -F -q 'DBF d0,.loop' "$DRIVER_SOURCE"
grep -F -q 'AHIDB_MaxChannels' "$DRIVER_SOURCE"

for freq in 10000 11000 12000 13000 14000 17640 18900 19200 22050 27348 32000 33075 37800 44100 48000 63000 88200 96000; do
	grep -q "$freq" "$DRIVER_SOURCE"
done

grep -q '_LVOAHIsub_Start EQU -54' "$COMPAT_INCLUDE/lvos/ahi_sub_lib.i"
grep -q '_LVOAHIsub_Update EQU -60' "$COMPAT_INCLUDE/lvos/ahi_sub_lib.i"
grep -q '_LVOAHIsub_Stop EQU -66' "$COMPAT_INCLUDE/lvos/ahi_sub_lib.i"
grep -F -q 'CUSTOM EQU $dff000' "$COMPAT_INCLUDE/hardware/all.i"

grep -q 'dc.l[[:space:]]\+AHIDB_HiFi,TRUE' "$MODE_SOURCE"
grep -q 'dc.l[[:space:]]\+AHIDB_Bits,16' "$MODE_SOURCE"
grep -F -q '$001a0000' "$MODE_SOURCE"
grep -q 'UAE :16 bit HIFI Stereo++' "$MODE_SOURCE"
if grep -q 'AHIDB_PingPong\|0x001a0002\|UAE: HiFi stereo' "$MODE_SOURCE"; then
	echo "baseline UAE AudioMode should match the original single WinUAE mode" >&2
	exit 1
fi
