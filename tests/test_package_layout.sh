#!/bin/sh
set -eu

PACKAGE_DIR="${PACKAGE_DIR:-build/package-test}"
PACKAGE_ROOT="${PACKAGE_DIR}/Host-Tools"

clean_dry_run="$(make -n clean)"
case "$clean_dry_run" in
	*"tests/test_package_layout.sh"*)
		echo "make clean must not remove checked-in shell tests" >&2
		exit 1
		;;
esac

rm -rf "$PACKAGE_DIR"
make package-dir PACKAGE_DIR="$PACKAGE_DIR"

test -d "$PACKAGE_ROOT"
test -f "${PACKAGE_ROOT}.info"
test -f "$PACKAGE_ROOT/Install"
test -f "$PACKAGE_ROOT/Install.info"
test -f "$PACKAGE_ROOT/README"
test -f "$PACKAGE_ROOT/README.info"
test -f "$PACKAGE_ROOT/Help.info"
cmp package/icons/drawer.info "${PACKAGE_ROOT}.info"
cmp package/icons/Help.info "$PACKAGE_ROOT/Help.info"
cmp package/icons/Install.info "$PACKAGE_ROOT/Install.info"
cmp package/icons/readme.info "$PACKAGE_ROOT/README.info"

if ! strings "$PACKAGE_ROOT/Install.info" | grep -q '^MINUSER=NOVICE$'; then
	echo "Installer icon must allow Novice mode" >&2
	exit 1
fi

if strings "$PACKAGE_ROOT/Install.info" | grep -q '^MINUSER=AVERAGE$'; then
	echo "Installer icon must not disable Novice mode with MINUSER=AVERAGE" >&2
	exit 1
fi

if ! strings "$PACKAGE_ROOT/Install.info" | grep -q '^NOLOG$'; then
	echo "Installer icon must disable automatic log file creation" >&2
	exit 1
fi

if strings "$PACKAGE_ROOT/Install.info" | grep -q '^LOGFILE='; then
	echo "Installer icon must not force a log file" >&2
	exit 1
fi

for icon in "${PACKAGE_ROOT}.info" "$PACKAGE_ROOT/Help.info" "$PACKAGE_ROOT/Install.info" "$PACKAGE_ROOT/README.info"; do
	magic="$(od -An -tx1 -N2 "$icon" | tr -d ' \n')"
	if [ "$magic" != "e310" ]; then
		echo "$icon is not a Workbench icon file" >&2
		exit 1
	fi
done

icon_current_position_hex() {
	od -An -tx1 -j58 -N8 "$1" | tr -d ' \n'
}

if [ "$(icon_current_position_hex "$PACKAGE_ROOT/Install.info")" != "0000002000000020" ]; then
	echo "Install.info must have a stable non-overlapping Workbench position" >&2
	exit 1
fi

if [ "$(icon_current_position_hex "$PACKAGE_ROOT/README.info")" != "0000009000000020" ]; then
	echo "README.info must have a stable non-overlapping Workbench position" >&2
	exit 1
fi

if [ "$(icon_current_position_hex "$PACKAGE_ROOT/Help.info")" != "0000010000000020" ]; then
	echo "Help.info must have a stable non-overlapping Workbench position" >&2
	exit 1
fi

drawer_flags_view_hex() {
	od -An -tx1 -j927 -N6 "$1" | tr -d ' \n'
}

if [ "$(drawer_flags_view_hex "${PACKAGE_ROOT}.info")" != "000003010001" ]; then
	echo "Top-level drawer icon must default to Show Only Icons and View By Icon" >&2
	exit 1
fi

if [ "$(drawer_flags_view_hex "$PACKAGE_ROOT/Help.info")" != "000003010001" ]; then
	echo "Help drawer icon must default to Show Only Icons and View By Icon" >&2
	exit 1
fi

drawer_window_hex() {
	od -An -tx1 -j78 -N8 "$1" | tr -d ' \n'
}

if [ "$(drawer_window_hex "${PACKAGE_ROOT}.info")" != "0050002801900096" ]; then
	echo "Top-level drawer icon must open a compact Workbench window" >&2
	exit 1
fi

if [ "$(drawer_window_hex "$PACKAGE_ROOT/Help.info")" != "0050002801900096" ]; then
	echo "Help drawer icon must open a compact Workbench window" >&2
	exit 1
fi

for tool in host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info host-download host-env; do
	test -f "$PACKAGE_ROOT/C/$tool"
done
grep -a -q '\$VER: Host-Run 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-run"
grep -a -q '\$VER: Host-MultiView 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-multiview"
grep -a -q '\$VER: Host-Shell 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-shell"
grep -a -q '\$VER: Host-Path 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-path"
grep -a -q '\$VER: Host-Reveal 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-reveal"
grep -a -q '\$VER: Host-Notify 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-notify"
grep -a -q '\$VER: Host-Edit 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-edit"
grep -a -q '\$VER: Host-Clip 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-clip"
grep -a -q '\$VER: Host-Info 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-info"
grep -a -q '\$VER: Host-Download 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-download"
grep -a -q '\$VER: Host-Env 2.4 (2026-06-10)' "$PACKAGE_ROOT/C/host-env"
if grep -a '\$VER: Host-[A-Za-z]* v[0-9]' "$PACKAGE_ROOT/C/"*; then
	echo "Host command \$VER strings must be parseable by Installer getversion, without a v prefix before the version number" >&2
	exit 1
fi

if [ -e "$PACKAGE_ROOT/Docs/Host-Tools/README.md" ]; then
	echo "Package must not duplicate the top-level README under Docs" >&2
	exit 1
fi

test -f "$PACKAGE_ROOT/Help/Host-Tools.guide"
test -f "$PACKAGE_ROOT/Help/Host-Tools.guide.info"
if [ -e "$PACKAGE_ROOT/Help/English" ]; then
	echo "Package Help drawer must contain the guide directly, not an English subdrawer" >&2
	exit 1
fi
cmp package/icons/guide.info "$PACKAGE_ROOT/Help/Host-Tools.guide.info"
grep -q '^@database' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'host-run' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'host-shell' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'host-clip' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'host-env' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'UAE :16 bit HIFI Stereo++' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'uaesnd: Stereo' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'uaesnd: HiFi Stereo' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'uaesnd: 7.1' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'UAE MHI MP3 Decoder' "$PACKAGE_ROOT/Help/Host-Tools.guide"
grep -q 'Libs/MHI/mhiuae.library' "$PACKAGE_ROOT/Help/Host-Tools.guide"

for icon in "$PACKAGE_ROOT/Help/Host-Tools.guide.info"; do
	magic="$(od -An -tx1 -N2 "$icon" | tr -d ' \n')"
	if [ "$magic" != "e310" ]; then
		echo "$icon is not a Workbench icon file" >&2
		exit 1
	fi
done

grep -q "Command tools" "$PACKAGE_ROOT/Install"
grep -q "AmigaGuide documentation" "$PACKAGE_ROOT/Install"
grep -q "HELP:" "$PACKAGE_ROOT/Install"
grep -q "Language" "$PACKAGE_ROOT/Install"
grep -q "host-run" "$PACKAGE_ROOT/Install"
grep -q "(set @app-name \"Host-Tools\")" "$PACKAGE_ROOT/Install"
grep -q "(set @default-dest \"\")" "$PACKAGE_ROOT/Install"
grep -q "(set #install-components" "$PACKAGE_ROOT/Install"
grep -q "(askoptions" "$PACKAGE_ROOT/Install"
grep -q "(= @user-level 0)" "$PACKAGE_ROOT/Install"
grep -q "(= @user-level 1)" "$PACKAGE_ROOT/Install"
grep -q "(= @user-level 2)" "$PACKAGE_ROOT/Install"
grep -q "(set #install-components 31)" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_ReplaceFile" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_EnsureDir" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopyVersionedFileAutomatic" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopyPlainFileAutomatic" "$PACKAGE_ROOT/Install"
grep -q '"Command tools"' "$PACKAGE_ROOT/Install"
grep -q '"AmigaGuide documentation"' "$PACKAGE_ROOT/Install"
grep -q '"UAE AHI audio driver"' "$PACKAGE_ROOT/Install"
grep -q '"UAE MHI MP3 decoder"' "$PACKAGE_ROOT/Install"
grep -q "Command tools: host commands to C:" "$PACKAGE_ROOT/Install"
grep -q "Docs: Host-Tools.guide to HELP:" "$PACKAGE_ROOT/Install"
grep -q "UAE AHI: uae.audio and UAE AudioMode." "$PACKAGE_ROOT/Install"
grep -q "MHI MP3: mhiuae.library to LIBS:MHI." "$PACKAGE_ROOT/Install"
if grep -q "Intermediate installs selected components automatically" "$PACKAGE_ROOT/Install"; then
	echo "Installer component checklist prompt should not explain standard Intermediate/Expert behavior" >&2
	exit 1
fi
grep -q "(BITAND #install-components 1)" "$PACKAGE_ROOT/Install"
grep -q "(BITAND #install-components 2)" "$PACKAGE_ROOT/Install"
grep -q "(BITAND #install-components 4)" "$PACKAGE_ROOT/Install"
grep -q "(BITAND #install-components 16)" "$PACKAGE_ROOT/Install"
if grep -q "(IN #install-components" "$PACKAGE_ROOT/Install"; then
	echo "Installer component checks should use BITAND for compatibility with common Installer scripts" >&2
	exit 1
fi
if grep -q 'Command tools - host-run' "$PACKAGE_ROOT/Install"; then
	echo "Installer checkbox labels must stay short to avoid crashing older Installer requesters" >&2
	exit 1
fi
grep -q "(procedure P_InstallCommandTool" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallGuide" "$PACKAGE_ROOT/Install"
grep -q "(copylib" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopyVersionedFile" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopyPlainFile" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopySelectedVersionedFile" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_CopySelectedPlainFile" "$PACKAGE_ROOT/Install"
grep -q "(getversion #_source)" "$PACKAGE_ROOT/Install"
grep -q "(getversion #_destfile)" "$PACKAGE_ROOT/Install"
grep -q "Installed version:" "$PACKAGE_ROOT/Install"
grep -q "Package version:" "$PACKAGE_ROOT/Install"
grep -q "Installed version: unavailable" "$PACKAGE_ROOT/Install"
grep -q "Package version: unavailable" "$PACKAGE_ROOT/Install"
grep -q "Installer could not read the installed file version, so Replace is selected by default." "$PACKAGE_ROOT/Install"
grep -q "Replace anyway" "$PACKAGE_ROOT/Install"
grep -q "The package file is newer, so Replace is selected by default." "$PACKAGE_ROOT/Install"
grep -q "The installed file is the same version or newer, so Skip is selected by default." "$PACKAGE_ROOT/Install"
grep -q "already exists. Replace it?" "$PACKAGE_ROOT/Install"
grep -q "(P_CopySelectedVersionedFile (cat \"Host command tool \" #_toolname)" "$PACKAGE_ROOT/Install"
grep -q "(P_CopySelectedPlainFile \"Host-Tools.guide\"" "$PACKAGE_ROOT/Install"
grep -q '"Help/Host-Tools.guide"' "$PACKAGE_ROOT/Install"
grep -q '"Help/Host-Tools.guide.info"' "$PACKAGE_ROOT/Install"
if grep -q '"Help/English/Host-Tools.guide' "$PACKAGE_ROOT/Install"; then
	echo "Installer source should use the package Help drawer guide directly" >&2
	exit 1
fi
if ! grep -q "^(exit)" "$PACKAGE_ROOT/Install"; then
	echo "Installer must exit without adding an extra completion page" >&2
	exit 1
fi

for tool in host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info host-download host-env; do
	grep -q "(P_InstallCommandTool \"$tool\")" "$PACKAGE_ROOT/Install"
done

if grep -q "(complete" "$PACKAGE_ROOT/Install"; then
	echo "Installer must use exit for the final message; complete is only progress reporting" >&2
	exit 1
fi

if grep -q "installation is complete" "$PACKAGE_ROOT/Install"; then
	echo "Installer must not add a redundant completion message page" >&2
	exit 1
fi

if grep -q "(source \"C\")" "$PACKAGE_ROOT/Install"; then
	echo "Installer must use version-aware per-tool copylib calls instead of bulk-copying C" >&2
	exit 1
fi

if grep -q "(source \"Help/English\")" "$PACKAGE_ROOT/Install"; then
	echo "Installer must copy the AmigaGuide explicitly instead of bulk-copying Help/English" >&2
	exit 1
fi

if grep -q "(askbool" "$PACKAGE_ROOT/Install"; then
	echo "Installer component selection should use one descriptive checklist instead of isolated yes/no prompts" >&2
	exit 1
fi

old_hyphenated="Amiberry""-Host-Tools"
old_spaced="Amiberry"" Host Tools"
if grep -R "$old_hyphenated\|$old_spaced" "$PACKAGE_ROOT"; then
	echo "Package contents must use Host-Tools naming" >&2
	exit 1
fi

if grep -R -E "Picasso96|P96" "$PACKAGE_ROOT"; then
	echo "Package contents must not reference P96 while graphics driver support is out of scope" >&2
	exit 1
fi

if grep -E "User-Startup|ScreenMode|Prefs:" "$PACKAGE_ROOT/Install"; then
	echo "Installer must not modify startup or prefs state" >&2
	exit 1
fi

test -f "$PACKAGE_ROOT/Devs/AHI/uae.audio"
test -f "$PACKAGE_ROOT/Devs/AudioModes/UAE"
grep -a -q 'uae 4.4 (11.8.04)' "$PACKAGE_ROOT/Devs/AHI/uae.audio"
grep -a -q 'ahi_winuae' "$PACKAGE_ROOT/Devs/AHI/uae.audio"
mode_magic="$(od -An -tx1 -N4 "$PACKAGE_ROOT/Devs/AudioModes/UAE" | tr -d ' \n')"
if [ "$mode_magic" != "464f524d" ]; then
	echo "$PACKAGE_ROOT/Devs/AudioModes/UAE is not an IFF FORM mode file" >&2
	exit 1
fi
expected_mode_hex="464f524d0000007a4148494d4155444e00000004756165004155444d0000006280000064001a00008000006e000000108000006700000001800000680000000180000069000000018000006a000000018000006c000000008000806d000000480000000000000000554145203a31362062697420484946492053746572656f2b2b00"
actual_mode_hex="$(od -An -tx1 "$PACKAGE_ROOT/Devs/AudioModes/UAE" | tr -d ' \n')"
if [ "$actual_mode_hex" != "$expected_mode_hex" ]; then
	echo "$PACKAGE_ROOT/Devs/AudioModes/UAE does not match the original WinUAE AudioMode bytes" >&2
	exit 1
fi
grep -q "UAE AHI audio driver" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallAHIDriver" "$PACKAGE_ROOT/Install"
grep -q '"Devs/AHI/uae.audio"' "$PACKAGE_ROOT/Install"
grep -q '"DEVS:AHI"' "$PACKAGE_ROOT/Install"
grep -q '"Devs/AudioModes/UAE"' "$PACKAGE_ROOT/Install"
grep -q '"DEVS:AudioModes"' "$PACKAGE_ROOT/Install"

test -f "$PACKAGE_ROOT/Devs/AHI/uaesnd.audio"
test -f "$PACKAGE_ROOT/Devs/AudioModes/UAESND"
grep -a -q 'uaesnd.audio 4.1' "$PACKAGE_ROOT/Devs/AHI/uaesnd.audio"
v2_mode_magic="$(od -An -tx1 -N4 "$PACKAGE_ROOT/Devs/AudioModes/UAESND" | tr -d ' \n')"
if [ "$v2_mode_magic" != "464f524d" ]; then
	echo "$PACKAGE_ROOT/Devs/AudioModes/UAESND is not an IFF FORM mode file" >&2
	exit 1
fi
grep -a -q 'uaesnd: Stereo' "$PACKAGE_ROOT/Devs/AudioModes/UAESND"
grep -a -q 'uaesnd: HiFi Stereo' "$PACKAGE_ROOT/Devs/AudioModes/UAESND"
grep -a -q 'uaesnd: 7.1' "$PACKAGE_ROOT/Devs/AudioModes/UAESND"
grep -q "UAESND AHI audio driver" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallUAESNDDriver" "$PACKAGE_ROOT/Install"
grep -q '"Devs/AHI/uaesnd.audio"' "$PACKAGE_ROOT/Install"
grep -q '"Devs/AudioModes/UAESND"' "$PACKAGE_ROOT/Install"
grep -q "(BITAND #install-components 8)" "$PACKAGE_ROOT/Install"
grep -q "(default 31)" "$PACKAGE_ROOT/Install"
grep -q "UAESND AHI: uaesnd.audio and UAESND AudioMode." "$PACKAGE_ROOT/Install"

test -f "$PACKAGE_ROOT/Libs/MHI/mhiuae.library"
grep -a -q '\$VER: mhiuae.library 2.4 (2026-06-10)' "$PACKAGE_ROOT/Libs/MHI/mhiuae.library"
grep -q "UAE MHI MP3 decoder library" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallMHILibrary" "$PACKAGE_ROOT/Install"
grep -q '"Libs/MHI/mhiuae.library"' "$PACKAGE_ROOT/Install"
grep -q '"LIBS:MHI"' "$PACKAGE_ROOT/Install"
grep -q '(P_EnsureDir "LIBS:MHI")' "$PACKAGE_ROOT/Install"
grep -q "(makedir #_destdir" "$PACKAGE_ROOT/Install"

package_dry_run="$(make -n package PACKAGE_DIR="${PACKAGE_DIR}-dryrun")"
case "$package_dry_run" in
	*"Host-Tools-"*".lha"*) ;;
	*)
		echo "Package archive name must use Host-Tools-<version>.lha" >&2
		exit 1
		;;
esac

case "$package_dry_run" in
	*"Host-Tools.info"*) ;;
	*)
		echo "Package archive must include the top-level drawer icon" >&2
		exit 1
		;;
esac
