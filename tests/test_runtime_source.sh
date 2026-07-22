#!/bin/sh
set -eu

CAPTURE_HEADER="src/host_capture.h"
DOWNLOAD_SOURCE="src/host-download.c"
SHELL_SOURCE="src/host-shell.c"
MHI_HEADER="drivers/mhi/src/mhiuae.h"
MHI_SOURCE="drivers/mhi/src/mhiuae.c"

grep -F -q '#define HOST_CAPTURE_STATUS_IDLE_LIMIT 1500' "$CAPTURE_HEADER"
grep -F -q 'SetSignal(0, 0) & SIGBREAKF_CTRL_C' "$CAPTURE_HEADER"
grep -F -q 'Aborted host command.' "$CAPTURE_HEADER"

grep -F -q 'static char session_command[HOST_MAX_COMMAND_LEN];' "$SHELL_SOURCE"

grep -F -q "make_unused_sidecar_path(destpath, 'p'" "$DOWNLOAD_SOURCE"
grep -F -q 'install_download(temp_path, destpath, force' "$DOWNLOAD_SOURCE"
if grep -F -q 'Open((STRPTR)destpath, MODE_NEWFILE)' "$DOWNLOAD_SOURCE"; then
	echo "host-download must write to a sidecar before replacing the destination" >&2
	exit 1
fi

grep -F -q 'UaeMHIAlloc(task, sigmask)' "$MHI_SOURCE"
grep -F -q 'return UaeMHIQueue(player->host_handle, buffer, size, (ULONG)buffer) ? TRUE : FALSE;' "$MHI_SOURCE"
if grep -F -q 'Signal(player->task' "$MHI_SOURCE"; then
	echo "MHI completion signals must come from the host after buffer consumption" >&2
	exit 1
fi
if grep -q 'struct Task \*task;\|ULONG sigmask;' "$MHI_HEADER"; then
	echo "MHI player must not retain completion state used for eager signalling" >&2
	exit 1
fi
