<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Amiberry Host Tools

A collection of AmigaOS tools designed to bridge the gap between the Amiberry emulator and the host operating system (Linux, macOS, etc.).

These tools allow you to execute commands, open files, and launch interactive shells on the host system directly from the Amiga environment.

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

**Usage:**
```shell
host-shell
```

---

## Requirements

-   **Amiberry v6.0+** (or a version with updated `uaelib` support).
-   "Native Code" execution must be enabled in Amiberry settings.

## Installation

1.  Download the latest release from the [Releases Page](../../releases).
2.  Extract the `.lha` archive.
3.  Copy the binaries (`host-run`, `host-multiview`, `host-shell`) to `C:` or anywhere in your system path.

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

To build with debug output enabled:
```shell
make debug
```

## License
Copyright (C) 2020-2026 Dimitris Panokostas.

Amiberry Host Tools is licensed under the GNU General Public License version 3 or later. See [LICENSE](LICENSE) for details.
