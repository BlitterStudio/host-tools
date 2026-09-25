<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Getting started

## Requirements

Run the classic AmigaOS 68k tools in Amiberry v6.0+ or a build with the required `uae.resource`/`uaelib` support. Enable **Native Code** execution for the command tools and MHI decoder. To use the UAESND AHI driver, also enable the **UAESND sound board** in Amiberry. The two AHI drivers are guest audio components; the host support table below describes the **command tools**, not the audio drivers.

| Host | Supported commands |
| --- | --- |
| Linux and macOS | All 11 commands |
| Windows (current Amiberry) | `host-path`, `host-reveal`, `host-clip`, `host-info`, `host-download`, `host-env` |

Linux desktop integration uses `xdg-utils`, `notify-send`, and, depending on the operation, `gtk-launch`, `gdbus`, `iconv`, or a clipboard backend (`wl-clipboard`, `xclip`, `xsel`). Downloads use host `curl` or `wget`; on Windows they use `curl.exe` (included in Windows 10 and later). Windows desktop integrations use PowerShell and Explorer. Check the [command reference](Command-Tools) for individual requirements and exit codes.

## Install from a release

1. Download the latest `Host-Tools-<version>.lha` from the [releases page](https://github.com/BlitterStudio/host-tools/releases); a `.lha.sha256` checksum is published beside it.
2. Extract the archive and open the `Host-Tools` drawer.
3. Run `Install`. All five components are selected by default: command tools, AmigaGuide, UAE AHI, UAESND AHI, and UAE MHI MP3.

**Novice** installs everything and replaces existing files. **Intermediate** lets you select components and copies files automatically. **Expert** lets you select components and asks before replacing files, showing installed and package versions when available. The installer does not change startup scripts, system settings, AHI preferences, or existing driver configuration. It installs the guide to `HELP:<language>/Host-Tools.guide`, using `English` if no language is set.

For manual installation, copy the command programs from the archive's `C/` drawer to `C:` or another directory on the Amiga command path. Copy each audio component's files to the locations in [Audio and MP3](Audio-and-MP3). The AmigaGuide can be copied to `HELP:<language>/`.

## Try the host commands

These are Amiga CLI examples. Files must already exist where the example expects them; host applications can use Amiga paths backed by host-visible mounts.

Open a URL in the Linux or macOS host browser:

```text
host-multiview https://github.com/BlitterStudio/amiberry
```

Open a video on a host-visible Amiga volume using the host's default application (Linux/macOS):

```text
host-multiview "Work:Videos/My Holiday.mp4"
```

To use a particular host application instead, run it explicitly (Linux/macOS, with the application installed on the host):

```text
host-run vlc "Work:Videos/My Holiday.mp4"
```

Download an archive onto the Amiga RAM disk on any supported host:

```text
host-download https://github.com/BlitterStudio/host-tools/releases/download/v2.6/Host-Tools-2.6.lha RAM:
```

To open a class of files on the host from Workbench, set its **DefIcons** default tool to `host-multiview` (Linux/macOS). `host-path Work:Videos` prints the host path for a mounted directory when you need to check where it resolves.

For usage and behavior of the remaining commands, see [Command tools](Command-Tools). For choosing and installing audio modes, see [Audio and MP3](Audio-and-MP3).
