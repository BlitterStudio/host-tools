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

for icon in "${PACKAGE_ROOT}.info" "$PACKAGE_ROOT/Help.info" "$PACKAGE_ROOT/Install.info" "$PACKAGE_ROOT/README.info"; do
	magic="$(od -An -tx1 -N2 "$icon" | tr -d ' \n')"
	if [ "$magic" != "e310" ]; then
		echo "$icon is not a Workbench icon file" >&2
		exit 1
	fi
done

for tool in host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info; do
	test -f "$PACKAGE_ROOT/C/$tool"
done

if [ -e "$PACKAGE_ROOT/Docs/Host-Tools/README.md" ]; then
	echo "Package must not duplicate the top-level README under Docs" >&2
	exit 1
fi

test -f "$PACKAGE_ROOT/Help/English/Host-Tools.guide"
test -f "$PACKAGE_ROOT/Help/English/Host-Tools.guide.info"
cmp package/icons/guide.info "$PACKAGE_ROOT/Help/English/Host-Tools.guide.info"
grep -q '^@database' "$PACKAGE_ROOT/Help/English/Host-Tools.guide"
grep -q 'host-run' "$PACKAGE_ROOT/Help/English/Host-Tools.guide"
grep -q 'host-shell' "$PACKAGE_ROOT/Help/English/Host-Tools.guide"
grep -q 'host-clip' "$PACKAGE_ROOT/Help/English/Host-Tools.guide"
grep -q 'UAE :16 bit HIFI Stereo++' "$PACKAGE_ROOT/Help/English/Host-Tools.guide"

for icon in "$PACKAGE_ROOT/Help/English/Host-Tools.guide.info"; do
	magic="$(od -An -tx1 -N2 "$icon" | tr -d ' \n')"
	if [ "$magic" != "e310" ]; then
		echo "$icon is not a Workbench icon file" >&2
		exit 1
	fi
done

grep -q "Install Host command tools" "$PACKAGE_ROOT/Install"
grep -q "Install AmigaGuide documentation" "$PACKAGE_ROOT/Install"
grep -q "HELP:" "$PACKAGE_ROOT/Install"
grep -q "Language" "$PACKAGE_ROOT/Install"
grep -q "host-run" "$PACKAGE_ROOT/Install"
grep -q "(set @app-name \"Host-Tools\")" "$PACKAGE_ROOT/Install"
grep -q "(set @default-dest \"\")" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallCommandTool" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallGuide" "$PACKAGE_ROOT/Install"
grep -q "(copylib" "$PACKAGE_ROOT/Install"
grep -q "(exists (tackon \"C:\" #_toolname))" "$PACKAGE_ROOT/Install"
grep -q "(exists (tackon #help-dest \"Host-Tools.guide\"))" "$PACKAGE_ROOT/Install"
grep -q "(source \"Help/English/Host-Tools.guide\")" "$PACKAGE_ROOT/Install"
grep -q "(source \"Help/English/Host-Tools.guide.info\")" "$PACKAGE_ROOT/Install"
if ! grep -q "(exit" "$PACKAGE_ROOT/Install"; then
	echo "Installer must use exit for the final message" >&2
	exit 1
fi

for tool in host-run host-multiview host-shell host-path host-reveal host-notify host-edit host-clip host-info; do
	grep -q "(P_InstallCommandTool \"$tool\")" "$PACKAGE_ROOT/Install"
done

if grep -q "(complete" "$PACKAGE_ROOT/Install"; then
	echo "Installer must use exit for the final message; complete is only progress reporting" >&2
	exit 1
fi

if grep -q "(source \"C\")" "$PACKAGE_ROOT/Install"; then
	echo "Installer must use version-aware per-tool copylib calls instead of bulk-copying C" >&2
	exit 1
fi

if grep -q "(source \"Help/English\")" "$PACKAGE_ROOT/Install"; then
	echo "Installer must copy the versioned AmigaGuide explicitly instead of bulk-copying Help/English" >&2
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
grep -q "Install UAE AHI audio driver" "$PACKAGE_ROOT/Install"
grep -q "(procedure P_InstallAHIDriver" "$PACKAGE_ROOT/Install"
grep -q "(source \"Devs/AHI/uae.audio\")" "$PACKAGE_ROOT/Install"
grep -q "(dest \"DEVS:AHI\")" "$PACKAGE_ROOT/Install"
grep -q "(source \"Devs/AudioModes/UAE\")" "$PACKAGE_ROOT/Install"
grep -q "(dest \"DEVS:AudioModes\")" "$PACKAGE_ROOT/Install"

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
