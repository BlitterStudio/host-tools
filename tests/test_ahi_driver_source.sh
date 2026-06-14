#!/bin/sh
set -eu

MAKEFILE="drivers/ahi/Makefile"
ROOT_MAKEFILE="Makefile"
V1_SOURCE_DIR="drivers/ahi/src/v1"
DRIVER_SOURCE="$V1_SOURCE_DIR/uae.audio.asm"
MODE_SOURCE="$V1_SOURCE_DIR/UAE.asm"
COMPAT_INCLUDE="$V1_SOURCE_DIR/include"
V2_SOURCE_DIR="drivers/ahi/src/v2"
V2_DRIVER_SOURCE="$V2_SOURCE_DIR/uaesnd.audio.asm"
V2_MODE_SOURCE="$V2_SOURCE_DIR/UAESND.asm"

test -f "$MAKEFILE"
test -f "$ROOT_MAKEFILE"
test -d "$V1_SOURCE_DIR"
test -f "$DRIVER_SOURCE"
test -f "$MODE_SOURCE"
test -f "$COMPAT_INCLUDE/hardware/all.i"
test -f "$COMPAT_INCLUDE/lvos/ahi_sub_lib.i"
test -f "$COMPAT_INCLUDE/macros.i"
test -d "$V2_SOURCE_DIR"
test -f "$V2_DRIVER_SOURCE"
test -f "$V2_MODE_SOURCE"

grep -q 'VASMM68K' "$MAKEFILE"
grep -q 'V1_SOURCE_DIR[[:space:]]*= src/v1' "$MAKEFILE"
grep -q 'V1_DRIVER[[:space:]]*= $(V1_SOURCE_DIR)/uae.audio.asm' "$MAKEFILE"
grep -q 'V1_MODE[[:space:]]*= $(V1_SOURCE_DIR)/UAE.asm' "$MAKEFILE"
grep -q 'V2_SOURCE_DIR[[:space:]]*= src/v2' "$MAKEFILE"
grep -q 'V2_DRIVER[[:space:]]*= $(V2_SOURCE_DIR)/uaesnd.audio.asm' "$MAKEFILE"
grep -q 'V2_MODE[[:space:]]*= $(V2_SOURCE_DIR)/UAESND.asm' "$MAKEFILE"
grep -q '^ahi-v2:' "$MAKEFILE"
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

if find drivers/ahi/src -iname '*uaesnd*' ! -path "$V2_SOURCE_DIR/*" | grep .; then
	echo "UAESND sources must stay isolated under the experimental AHI v2 source tree" >&2
	exit 1
fi

grep -q 'AHI_V2_SOURCES' "$ROOT_MAKEFILE"
grep -q 'drivers/ahi/src/v2/UAESND.asm' "$ROOT_MAKEFILE"
grep -q 'drivers/ahi/package-v2/Devs/AHI/uaesnd.audio' "$ROOT_MAKEFILE"
grep -q 'drivers/ahi/package-v2/Devs/AudioModes/UAESND' "$ROOT_MAKEFILE"

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

grep -F -q 'dc.b "uaesnd.audio",0' "$V2_DRIVER_SOURCE"
grep -F -q 'FindConfigDev' "$V2_DRIVER_SOURCE"
grep -F -q 'include lvo/utility_lib.i' "$V2_DRIVER_SOURCE"
grep -F -q 'AHISF_KNOWSTEREO|AHISF_KNOWHIFI|AHISF_KNOWMULTICHANNEL' "$V2_DRIVER_SOURCE"
grep -F -q 'move.b #%101,set_intena(a4)' "$V2_DRIVER_SOURCE"
if grep -F -q 'btst #AHIACB_HIFI,d0' "$V2_DRIVER_SOURCE"; then
	echo "HiFi UAESND should use the same sample-start callback path as Stereo" >&2
	exit 1
fi
grep -F -q 'UWORD p_OutputVolume' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w #$8000,p_OutputVolume(a3)' "$V2_DRIVER_SOURCE"
grep -F -q 'mulu.w p_OutputVolume(a2),d0' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w d1,p_OutputVolume(a1)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w p_OutputVolume(a1),d0' "$V2_DRIVER_SOURCE"
grep -F -q 'sub.l #$8000,d1' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w d1,set_hpan(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w d1,STREAM_START+UAESNDSetCurrent+set_hpan(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.b #3,set_mask(a4)' "$V2_DRIVER_SOURCE"
grep -F -q 'or.b #%101,stream_master_intena(a5)' "$V2_DRIVER_SOURCE"
grep -F -q 'and.b #%101,d2' "$V2_DRIVER_SOURCE"
if grep -F -q 'set_hpan(a1)' "$V2_DRIVER_SOURCE"; then
	echo "UAESND v2 must leave sample set panning fields zeroed; the hardware rejects non-zero panning" >&2
	exit 1
fi
grep -F -q 'move.w ahiac_Channels(a2),d0' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w d0,p_StreamCnt(a3)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.w p_StreamCnt(a1),d1' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG p_PendingStartMask' "$V2_DRIVER_SOURCE"
grep -F -q 'or.l d0,base_stream_enable(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'or.l d0,base_stream_intena(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'or.l d0,p_PendingStartMask(a3)' "$V2_DRIVER_SOURCE"
grep -F -q 'flush_pending_starts' "$V2_DRIVER_SOURCE"
grep -F -q 'or.l d2,base_stream_enable(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l ch_set_current(a2),stream_sample_pointer_imm(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w cleanup_allocaudio_error' "$V2_DRIVER_SOURCE"
grep -F -q 'cleanup_allocaudio_error' "$V2_DRIVER_SOURCE"
grep -F -q 'tst.w p_DisableCount(a1)' "$V2_DRIVER_SOURCE"
grep -F -q 'beq.s .already_enabled' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.b	"uaesnd",0' "$V2_MODE_SOURCE"
grep -F -q 'AHIDB_AudioID,$003b0001' "$V2_MODE_SOURCE"
grep -F -q 'AHIDB_AudioID,$003b0002' "$V2_MODE_SOURCE"
grep -F -q 'AHIDB_AudioID,$003b0003' "$V2_MODE_SOURCE"
test "$(grep -F -c 'AHIDB_Panning,0' "$V2_MODE_SOURCE")" -eq 3
if grep -F -q 'AHIDB_Panning,1' "$V2_MODE_SOURCE"; then
	echo "UAESND v2 modes should expose fixed even/odd stereo routing, not panning" >&2
	exit 1
fi
grep -A8 -F 'AHIDB_AudioID,$003b0001' "$V2_MODE_SOURCE" | grep -F -q 'AHIDB_Bits,16'
grep -A10 -F 'AHIDB_AudioID,$003b0002' "$V2_MODE_SOURCE" | grep -F -q 'AHIDB_Bits,32'
grep -A10 -F 'AHIDB_AudioID,$003b0003' "$V2_MODE_SOURCE" | grep -F -q 'AHIDB_Bits,32'
test "$(grep -F -c 'AHIDB_Bits,16' "$V2_MODE_SOURCE")" -eq 1
test "$(grep -F -c 'AHIDB_Bits,32' "$V2_MODE_SOURCE")" -eq 2
grep -F -q 'move.l #UAESND_SIZEOF,p_DriverDataSize(a3)' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_CAP_CAPTURE EQU 8' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_CAP_CAPTURE_BLOCK EQU 16' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_CAPTURE_CONTROL_ENABLE EQU 1' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_CAPTURE_CONTROL_IRQ_ENABLE EQU 2' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_CAPTURE_BLOCK_COMMAND_COPY EQU 1' "$V2_DRIVER_SOURCE"
grep -F -q 'UAESND_RECORD_FRAMES EQU 2048' "$V2_DRIVER_SOURCE"
grep -F -q 'base_capture_block_address EQU $900' "$V2_DRIVER_SOURCE"
grep -F -q 'base_capture_block_frames EQU $904' "$V2_DRIVER_SOURCE"
grep -F -q 'base_capture_block_done EQU $908' "$V2_DRIVER_SOURCE"
grep -F -q 'base_capture_block_command EQU $90c' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG base_capabilities' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG base_capture_control' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG base_capture_intreq' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG base_capture_threshold' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG base_capture_available' "$V2_DRIVER_SOURCE"
grep -F -q 'ULONG p_Capabilities' "$V2_DRIVER_SOURCE"
grep -F -q 'APTR p_RecordBuffer' "$V2_DRIVER_SOURCE"
grep -F -q 'STRUCT p_RecordMessage,AHIRecordMessage_SIZEOF' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l base_capabilities(a4),p_Capabilities(a3)' "$V2_DRIVER_SOURCE"
grep -F -q 'or.l #AHISF_CANRECORD,d7' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_Record, 1' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_FullDuplex, 1' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_MaxRecordSamples, UAESND_RECORD_FRAMES' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_Inputs, 1' "$V2_DRIVER_SOURCE"
grep -F -q 'tag_input' "$V2_DRIVER_SOURCE"
grep -F -q 'btst #AHISB_RECORD,d2' "$V2_DRIVER_SOURCE"
grep -F -q 'btst #AHISB_PLAY,d2' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w start_playback' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w stop_playback' "$V2_DRIVER_SOURCE"
grep -F -q 'start_playback' "$V2_DRIVER_SOURCE"
grep -F -q 'stop_playback' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #UAESND_RECORD_BYTES,base_capture_threshold(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #UAESND_CAPTURE_CONTROL_ENABLE|UAESND_CAPTURE_CONTROL_IRQ_ENABLE,base_capture_control(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'btst #0,base_capture_status+3(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'bne.s .unavailable' "$V2_DRIVER_SOURCE"
grep -F -q 'tst.l base_capture_intreq(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'clr.l base_capture_intreq(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'clr.l base_capture_control(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'and.l #UAESND_CAP_CAPTURE_BLOCK,d1' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l p_RecordBuffer(a5),base_capture_block_address(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #UAESND_RECORD_FRAMES,base_capture_block_frames(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #UAESND_CAPTURE_BLOCK_COMMAND_COPY,base_capture_block_command(a0)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l base_capture_block_done(a0),d0' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #AHIST_S16S,p_RecordMessage+ahirm_Type(a1)' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l ahiac_SamplerFunc(a2),a0' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w process_recording' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w get_audioid_bits' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #AHIA_AudioID,d0' "$V2_DRIVER_SOURCE"
grep -F -q 'move.l #AHIDB_AudioID,d0' "$V2_DRIVER_SOURCE"
grep -F -q 'cmp.l #$003b0001,d0' "$V2_DRIVER_SOURCE"
grep -F -q 'moveq #16,d0' "$V2_DRIVER_SOURCE"
grep -F -q 'moveq #-1,d0' "$V2_DRIVER_SOURCE"
grep -F -q 'bsr.w get_maxchannels' "$V2_DRIVER_SOURCE"
grep -F -q 'move.b base_max_streams(a0),d2' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_Bits, 32' "$V2_DRIVER_SOURCE"
grep -F -q 'dc.l AHIDB_MaxChannels, 8' "$V2_DRIVER_SOURCE"
if grep -F -q 'cmp.l #$003b0002,d0' "$V2_DRIVER_SOURCE" || grep -F -q 'bsr.w get_audioid_maxchannels' "$V2_DRIVER_SOURCE"; then
	echo "UAESND v2 GetAttr must special-case only plain Stereo bits and leave HiFi on the static 32-bit path" >&2
	exit 1
fi
grep -F -q 'DEBUG EQU 0' "$V2_DRIVER_SOURCE"
grep -F -q 'IFNE DEBUG' "$V2_DRIVER_SOURCE"
if grep -F -q 'IFD DEBUG=1' "$V2_DRIVER_SOURCE"; then
	echo "UAESND v2 debug blocks must be value-gated so release builds do not write to DEBUG_ADDR" >&2
	exit 1
fi
