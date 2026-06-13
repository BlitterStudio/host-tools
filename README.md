<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Host-Tools

A collection of AmigaOS tools designed to bridge the gap between the Amiberry emulator and the host operating system (Linux, macOS, and Windows; see Requirements for per-tool host support).

These tools allow you to execute commands, open files, launch interactive shells, inspect host paths, and use common host desktop integrations directly from the Amiga environment.

## The Tools

### 1. host-run
**Execute host commands securely.**
`host-run` allows you to execute any command on the host system. It is designed to be robust and secure:
-   **Safe Quoting**: Arguments are automatically safeguarded, allowing you to pass complex filenames and parameters (including spaces and special characters) without issues.
-   **Path Translation**: Amiga paths (e.g., `Work:Documents/file.txt`) are automatically resolved to their host counterparts before execution.

**Usage:**
```shell
host-run <command> [arguments...]
```

### 2. host-multiview
**Open files or URLs with the default host application.**
Think of this as an "Open with..." command for the Amiga. It sends a file or URL to the host system, which then opens it using the default associated application (e.g., VLC for videos, Preview for images, your browser for URLs).
-   **Cross-Platform**: Works natively on **Linux** (via `xdg-open`) and **macOS** (via `open`).
-   **Seamless**: Perfect for integration with DefIcons to open media files or documents on the host.

**Usage:**
```shell
host-multiview <filename|URL> [filename2|URL2 ...]
```

### 3. host-shell
**Interactive Host Terminal.**
Opens a fully interactive terminal session on the host system, right inside your Amiga CLI.
-   **Interactive**: Supports `vi`, `htop`, and other interactive TUI applications.
-   **Shell Support**: Respects your host's default shell (Bash, Zsh, Fish, etc.).
-   **Raw Mode**: Provides a true terminal experience.
-   **Terminal Size**: The host terminal is sized to match the Amiga console window, so full-screen programs render correctly.

**Usage:**
```shell
host-shell [command]
```

### 4. host-path
**Print translated host paths.**
`host-path` resolves existing Amiga paths and prints their host-side paths. It is useful for debugging mounted directories and scripts.

**Usage:**
```shell
host-path <path> [path2 ...]
```

### 5. host-reveal
**Reveal files in the host file manager.**
`host-reveal` selects the file in Finder on macOS, in Explorer on Windows, or in the default Linux file manager through the `FileManager1` D-Bus interface (GNOME Files, Dolphin, Nemo, and others). When no compatible file manager is available, it opens the containing directory instead.

**Usage:**
```shell
host-reveal <path> [path2 ...]
```

### 6. host-notify
**Send host desktop notifications.**
`host-notify` uses `notify-send` on Linux or `osascript` on macOS when available. It is not yet available on Windows hosts.

**Usage:**
```shell
host-notify <message>
host-notify <title> <message...>
```

### 7. host-edit
**Open files in the host desktop editor.**
`host-edit` uses `open -t` on macOS and the host's default GUI text editor on Linux via `xdg-mime` and `gtk-launch`, with `xdg-open`, `VISUAL`, or `EDITOR` as fallbacks.

**Usage:**
```shell
host-edit <path> [path2 ...]
```

### 8. host-clip
**Use the host clipboard.**
`host-clip` copies text to the host clipboard or prints the current host clipboard contents. Without text arguments, `host-clip copy` reads standard input verbatim, so multi-line text and command output can be piped or redirected to the host clipboard. Text is converted between the Amiga's ISO-8859-1 character set and the host's encoding: through `iconv` on Linux and macOS, and inherently through PowerShell's Unicode pipeline on Windows.

**Usage:**
```shell
host-clip [copy] <text...>
host-clip copy < file
host-clip paste
```

### 9. host-info
**Print host integration details.**
`host-info` reports basic host OS, shell, editor, opener, and clipboard backend details.

**Usage:**
```shell
host-info
```

### 10. host-download
**Download files through the host.**
`host-download` fetches a URL with the host's `curl` (or `wget`) and saves it to any Amiga path — `RAM:`, hardfiles, and directory mounts all work, because the file is written by the tool through AmigaDOS. The host handles HTTPS/TLS, giving classic AmigaOS access to modern servers.
-   **Live Progress**: With a current Amiberry, the file streams to the Amiga as it downloads, with a percentage display when the server reports a size.
-   **Safe**: Failed or aborted downloads (Ctrl-C) never leave a partial file behind, and an existing destination is only overwritten with `FORCE`.
-   **Flexible Destination**: With no destination the file is saved in the current directory under its URL name; a directory destination keeps the URL name.

**Usage:**
```shell
host-download <URL> [<destination>] [FORCE]
```

### 11. host-env
**Get and set host user environment variables.**
`host-env` reads, writes, removes, and lists environment variables on the host. On Windows it updates the user's persistent environment. On Linux and macOS it writes persistent values to `$HOME/.host-tools-env`; source that file from your host shell startup files if you want future host login shells to import those values automatically.
-   **Persistent**: Changes are intended for future host processes.
-   **Scoped Safely**: Existing host shells, desktop apps, and Amiberry's parent process cannot have their live environment changed by a child process.

**Usage:**
```shell
host-env get NAME
host-env set NAME VALUE
host-env unset NAME
host-env list
```

---

## Requirements

-   **Amiberry v6.0+** (or a version with updated `uaelib` support).
-   "Native Code" execution must be enabled in Amiberry settings.
-   All tools work on **Linux and macOS hosts**. On **Windows hosts**, `host-path`, `host-download`, `host-clip`, `host-reveal`, `host-info`, and `host-env` are supported with a current Amiberry (PowerShell and Explorer handle the Windows side; `curl.exe` ships with Windows 10 and later). The remaining tools (`host-run`, `host-multiview`, `host-shell`, `host-edit`, `host-notify`) currently require a Linux or macOS host, since their host commands run through the POSIX shell.
-   For status-aware tools (`host-reveal`, `host-notify`, `host-clip`, and `host-info`), a newer Amiberry build with the `HostShell_Status` trap reports host command failures immediately. Older builds still work, but use timeout-based completion detection.
-   Linux desktop integration uses `xdg-utils` (`xdg-open`, `xdg-mime`) and GTK's `gtk-launch` when available. Notifications use `notify-send`; clipboard support uses `wl-clipboard`, `xclip`, or `xsel`; file selection in `host-reveal` uses `gdbus` when present. Character set conversion uses `iconv` when present.
-   `host-download` uses the host's `curl` or `wget` (`curl.exe` on Windows). Live streaming progress requires an Amiberry build with pipe-based HostShell sessions; on older Linux and macOS builds the tool falls back to a two-phase transfer that downloads on the host first.

## Exit Codes

All tools follow AmigaDOS conventions: `0` on success, `10` (`RETURN_ERROR`) when an operation fails or the tool is invoked with missing or invalid arguments, and `2` when `uae.resource` is unavailable (for example, when running outside Amiberry or with Native Code execution disabled). The explicit `?` help request returns `0`.

## Installation

1.  Download the latest release from the [Releases Page](../../releases).
2.  Extract the `Host-Tools-<version>.lha` archive.
3.  Open the `Host-Tools` drawer and run `Install`.

The installer shows a component checklist for the command tools, AmigaGuide documentation, and the UAE and UAESND AHI audio drivers. Before replacing an existing versioned file, it shows the installed and package versions and asks whether to replace or skip it. Files without version strings are still checked for existence and ask before overwrite. It does not edit startup files, system settings, AHI preferences, or existing driver configuration.

For manual installation, copy the binaries (`host-run`, `host-multiview`, `host-shell`, `host-path`, `host-reveal`, `host-notify`, `host-edit`, `host-clip`, `host-info`, `host-download`, `host-env`) from the package `C` drawer to `C:` or anywhere in your system path. To install the UAE AHI driver manually, copy `Devs/AHI/uae.audio` to `DEVS:AHI/` and `Devs/AudioModes/UAE` to `DEVS:AudioModes/`. To install the UAESND AHI driver manually, copy `Devs/AHI/uaesnd.audio` to `DEVS:AHI/` and `Devs/AudioModes/UAESND` to `DEVS:AudioModes/`. The UAESND driver plays each AHI channel through a hardware audio stream and requires the UAESND sound board to be enabled in the Amiberry configuration.

## Examples

### Web Browsing
Open a URL in the host's default browser (works on both Linux and macOS):
```shell
host-multiview https://github.com/BlitterStudio/amiberry
```
Or using `host-run` with the platform-specific command:
```shell
host-run xdg-open https://github.com/BlitterStudio/amiberry
```

### Playing Media
Play a video file located on an Amiga partition using the host's media player:
```shell
host-run vlc "Work:Videos/My Holiday.mp4"
```
Or simply:
```shell
host-multiview "Work:Videos/My Holiday.mp4"
```

### Downloading Files
Download an archive from Aminet straight to the RAM disk, with the host handling HTTPS:
```shell
host-download https://aminet.net/dev/misc/example.lha RAM:
```

### DefIcons Integration
Make AmigaOS automatically open `.mkv` files on the host:
1.  Open **DefIcons**.
2.  Add/Edit the `mkv` entry.
3.  Set the **Default Tool** to `host-multiview`.
4.  Now, double-clicking any MKV file in Workbench will play it on the host!

## Building

This project is built using **GitHub Actions**. Every push to `master` or a version tag triggers a build using the `sacredbanana/amiga-compiler:m68k-amigaos` Docker image.

To build locally (requires the m68k-amigaos cross-compiler):
```shell
make all
```

To build locally with the same Docker image used by CI:
```shell
docker run --rm -v "$PWD":/work -w /work sacredbanana/amiga-compiler:m68k-amigaos make all
```

To run the host-side command-builder tests:
```shell
make test
```

To build a release archive:
```shell
make package
```

The package target creates `Host-Tools-<version>.lha`, containing a structured `Host-Tools` drawer with the Installer script, command tools, README, AmigaGuide documentation, and the UAE and UAESND AHI driver files.

Native package builds also require `lha`. The Docker build image contains this tool.

To build with debug output enabled:
```shell
make debug
```

## License
Copyright (C) 2020-2026 Dimitris Panokostas.

Host-Tools is licensed under the GNU General Public License version 3 or later. See [LICENSE](LICENSE) for details.
