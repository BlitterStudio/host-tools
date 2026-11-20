<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Getting started

## Requirements

Run the tools in Amiberry v6.0 or newer, or in a build with the `uae.resource`/`uaelib` support the tools use. Enable **Native Code** execution for the command tools and the MHI decoder. `host-quit` is the exception: it needs only `uae.resource`. The UAESND AHI driver also needs the **UAESND sound board** enabled in Amiberry. The audio drivers run on the Amiga side, so the host requirements below apply to the command tools only.

All commands work on Linux, macOS and Windows hosts. On Windows, `host-shell` needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later, which provides the ConPTY pseudo console. The other commands work with a current Amiberry on every host.

The commands call these programs on the host:

- Linux: `xdg-utils` and `notify-send`, plus `gtk-launch`, `gdbus`, `iconv` or a clipboard backend (`wl-clipboard`, `xclip`, `xsel`) depending on the command.
- macOS: `open`, `osascript`, `pbcopy` and `pbpaste`.
- Windows: PowerShell, Explorer and Notepad.
- Downloads: `curl` or `wget` on Linux and macOS, `curl.exe` on Windows (included in Windows 10 and later).

The [command reference](Command-Tools) lists the requirements and exit codes of each command.

## Install from a release

1. Download the latest `Host-Tools-<version>.lha` from the [releases page](https://github.com/BlitterStudio/host-tools/releases). A `.lha.sha256` checksum is published beside it.
2. Extract the archive and open the `Host-Tools` drawer.
3. Run `Install`. All five components are selected by default: command tools, AmigaGuide, UAE AHI, UAESND AHI and UAE MHI MP3.

**Novice** installs everything and replaces existing files. **Intermediate** lets you select components and copies files without asking, but leaves an installed program or library alone when it is the same version or newer. **Expert** lets you select components and asks before replacing files, showing the installed and package versions when available. The installer does not change startup scripts, system settings, AHI preferences or existing driver configuration. It installs the guide to `HELP:<language>/Host-Tools.guide`, using `English` if no language is set.

To install by hand, copy the command programs from the archive's `C/` drawer to `C:` or another directory on the Amiga command path. Copy the audio files to the locations listed in [Audio and MP3](Audio-and-MP3). Copy the AmigaGuide to `HELP:<language>/`.

## Try the host commands

Type these in an Amiga CLI. A host application can open a file only if it sits in a host-visible mount, not in a hardfile.

Open a URL in the host browser:

```text
host-multiview https://github.com/BlitterStudio/amiberry
```

Open a video on a host-visible Amiga volume with the host's default application:

```text
host-multiview "Work:Videos/My Holiday.mp4"
```

To use a particular host application, run it by name. It must be installed on the host:

```text
host-run vlc "Work:Videos/My Holiday.mp4"
```

Download an archive to the Amiga RAM disk (this works on every supported host):

```text
host-download https://github.com/BlitterStudio/host-tools/releases/download/v2.6/Host-Tools-2.6.lha RAM:
```

To open a class of files on the host from Workbench, set its **DefIcons** default tool to `host-multiview`. `host-path Work:Videos` prints the host path of a mounted directory, which shows where it resolves.

The other commands are described in [Command tools](Command-Tools). Audio modes and drivers are in [Audio and MP3](Audio-and-MP3).
