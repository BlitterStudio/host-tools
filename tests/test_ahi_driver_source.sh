#!/bin/sh
set -eu

DRIVER_SOURCE="drivers/ahi/src/uae.audio.c"
GATES_SOURCE="drivers/ahi/src/uae-gates.S"
MODE_SOURCE="drivers/ahi/src/UAE.S"

test -f "$DRIVER_SOURCE"
test -f "$GATES_SOURCE"
test -f "$MODE_SOURCE"

grep -q '#define UAE_MAX_AHI_CHANNELS[[:space:]]\+128' "$DRIVER_SOURCE"
grep -q '#define UAE_BUFFER_BLOCKS[[:space:]]\+2' "$DRIVER_SOURCE"
grep -q '96000' "$DRIVER_SOURCE"
grep -q 'ahi_winuae' "$DRIVER_SOURCE"
grep -q 'uae_find_trap' "$DRIVER_SOURCE"
grep -q '_uae_find_trap' "$GATES_SOURCE"
grep -q 'moveq[[:space:]]*#6,d0' "$GATES_SOURCE"

if grep -q 'UAE_PRELOAD_BLOCKS' "$DRIVER_SOURCE"; then
	echo "driver must not preload by advancing AHI player hooks before host pacing begins"
	exit 1
fi

awk '
	/while \(data->running\)/ {
		in_playback_loop = 1
	}
	in_playback_loop && /uae_ahi_poll/ {
		print "driver playback loop must wait for host write interrupts, not poll the emulator clock"
		exit 1
	}
	in_playback_loop && /FreeSignal\(data->slavesignal\)/ {
		in_playback_loop = 0
	}
' "$DRIVER_SOURCE"

grep -q 'audioctrl->ahiac_BuffSamples[[:space:]]*=' "$DRIVER_SOURCE"
grep -q 'audioctrl->ahiac_BuffSize[[:space:]]*=' "$DRIVER_SOURCE"
grep -q 'ahi_call_pre_timer' "$DRIVER_SOURCE"
grep -q 'ahi_call_post_timer' "$DRIVER_SOURCE"
grep -q 'convert_hifi_to_host' "$DRIVER_SOURCE"
grep -q 'dst\[0\][[:space:]]*=[[:space:]]*src\[0\]' "$DRIVER_SOURCE"
grep -q 'dst\[1\][[:space:]]*=[[:space:]]*src\[2\]' "$DRIVER_SOURCE"
grep -q 'AHIACF_HIFI' "$DRIVER_SOURCE"
grep -q 'AHISF_KNOWHIFI' "$DRIVER_SOURCE"
grep -q 'LONG2[[:space:]]\+AHIDB_HiFi,[[:space:]]*TRUE' "$MODE_SOURCE"
grep -q 'UAE: HiFi stereo++' "$MODE_SOURCE"
grep -q 'UAE: HiFi stereo' "$MODE_SOURCE"
grep -q '0x001a0002' "$MODE_SOURCE"
grep -q '_ahi_call_pre_timer' "$GATES_SOURCE"
grep -q '_ahi_call_post_timer' "$GATES_SOURCE"
grep -q 'uae_ahi_write(data->base->trap_addr, UAE_UNIT, buffer)' "$DRIVER_SOURCE"
grep -q 'AddIntServer(INTB_EXTER, &data->playinterrupt)' "$DRIVER_SOURCE"
grep -q 'RemIntServer(INTB_EXTER, &data->playinterrupt)' "$DRIVER_SOURCE"
grep -q 'Cause(&data->softinterrupt)' "$DRIVER_SOURCE"
grep -q 'ahi_interrupt_entry' "$DRIVER_SOURCE"
grep -q 'ahi_softint_entry' "$DRIVER_SOURCE"
grep -q '_ahi_interrupt_entry' "$GATES_SOURCE"
grep -q '_ahi_softint_entry' "$GATES_SOURCE"
grep -q 'volatile UWORD disable_count' "$DRIVER_SOURCE"
grep -q 'data->disable_count++' "$DRIVER_SOURCE"
grep -q 'data->disable_count--' "$DRIVER_SOURCE"
grep -q 'data->disable_count != 0' "$DRIVER_SOURCE"
grep -q 'data->softinterrupt.is_Node.ln_Type = NT_INTERRUPT' "$DRIVER_SOURCE"

awk '
	/_uae_ahi_open:/ {
		in_func = 1
		next
	}
	in_func && /movem\.l[[:space:]]+d2-d5,-\(sp\)/ {
		saves = 1
	}
	in_func && /movem\.l[[:space:]]+\(sp\)\+,d2-d5/ {
		restores = 1
	}
	in_func && /^[[:space:]]*rts/ {
		exit !(saves && restores)
	}
	END {
		if (!saves || !restores) {
			exit 1
		}
	}
' "$GATES_SOURCE"

awk '
	/_ahi_call_pre_timer:/ {
		in_func = 1
		next
	}
	in_func && /movem\.l[[:space:]]+d2-d7\/a2-a6,-\(sp\)/ {
		saves = 1
	}
	in_func && /movem\.l[[:space:]]+\(sp\)\+,d2-d7\/a2-a6/ {
		restores = 1
	}
	in_func && /^[[:space:]]*rts/ {
		exit !(saves && restores)
	}
	END {
		if (!saves || !restores) {
			exit 1
		}
	}
' "$GATES_SOURCE"

awk '
	/_ahi_call_post_timer:/ {
		in_func = 1
		next
	}
	in_func && /movem\.l[[:space:]]+d2-d7\/a2-a6,-\(sp\)/ {
		saves = 1
	}
	in_func && /movem\.l[[:space:]]+\(sp\)\+,d2-d7\/a2-a6/ {
		restores = 1
	}
	in_func && /^[[:space:]]*rts/ {
		exit !(saves && restores)
	}
	END {
		if (!saves || !restores) {
			exit 1
		}
	}
' "$GATES_SOURCE"

if grep -q 'CreateNewProc\|SlaveEntry\|slavetask' "$DRIVER_SOURCE"; then
	echo "driver playback must use INTB_EXTER plus software interrupt, not a polling task"
	exit 1
fi

if grep -q 'UAE_OUTPUT_GAIN\|scale_host_sample\|amplify_host_buffer' "$DRIVER_SOURCE"; then
	echo "driver must not apply fixed gain; keep sample scaling aligned with original uae.audio"
	exit 1
fi

if grep -q 'Forbid()\|Permit()' "$DRIVER_SOURCE"; then
	echo "driver disable/enable must use local interrupt gating, not global Forbid/Permit"
	exit 1
fi

if grep -q 'AHIC_OutputVolume\|AHIC_MonitorVolume\|AHIDB_MinOutputVolume\|AHIDB_MaxOutputVolume' "$DRIVER_SOURCE"; then
	echo "driver must not advertise unsupported hardware volume controls"
	exit 1
fi

awk '
	/case AHIDB_MaxChannels:/ {
		in_case = 1
		next
	}
	in_case && /return UAE_MAX_AHI_CHANNELS;/ {
		found = 1
		exit 0
	}
	in_case && /case AHIDB_/ {
		exit 1
	}
	END {
		if (!found) {
			exit 1
		}
	}
' "$DRIVER_SOURCE"

awk '
	/case AHIDB_MaxPlaySamples:/ {
		in_case = 1
		next
	}
	in_case && /ahiac_BuffSamples[[:space:]]*\*[[:space:]]*UAE_BUFFER_BLOCKS/ {
		found = 1
		exit 0
	}
	in_case && /case AHIDB_/ {
		exit 1
	}
	END {
		if (!found) {
			exit 1
		}
	}
' "$DRIVER_SOURCE"
