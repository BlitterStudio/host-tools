<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Host-Tools

A classic AmigaOS 68k package for [Amiberry](https://github.com/BlitterStudio/amiberry). It includes 11 command-line tools for working with the host operating system, two AHI audio drivers, and an MHI MP3 decoder library. The guest tools run inside Amiberry; host integrations target Linux, macOS, or Windows as noted below.

## What's included

| Component | What it does | Installed location |
| --- | --- | --- |
| 11 command tools | Run host commands, open files and URLs, use the host shell, paths, desktop, clipboard, downloads, and environment | `C:` |
| UAE AHI driver | Classic UAE 16-bit stereo AHI output | `DEVS:AHI/uae.audio`, `DEVS:AudioModes/UAE` |
| UAESND AHI driver | AHI output through Amiberry's UAESND sound board | `DEVS:AHI/uaesnd.audio`, `DEVS:AudioModes/UAESND` |
| UAE MHI MP3 decoder | Offloads MP3 decoding to Amiberry | `LIBS:MHI/mhiuae.library` |
| AmigaGuide | On-Amiga reference for the tools and drivers | `HELP:<language>/Host-Tools.guide` |

The installer lets you select these components separately. The audio drivers and MHI library are not command-line tools; installing them does not change your audio preferences.

For detailed usage, see the [command reference](https://github.com/BlitterStudio/host-tools/wiki/Command-Tools), [audio and MP3 guide](https://github.com/BlitterStudio/host-tools/wiki/Audio-and-MP3), and [getting started guide](https://github.com/BlitterStudio/host-tools/wiki/Getting-Started) in the wiki. The package also includes an offline AmigaGuide. Wiki pages are authored in [`docs/wiki/`](docs/wiki/) alongside the code and published to the GitHub wiki.

## Requirements

-   **Amiberry v6.0+** (or a build with the required `uae.resource`/`uaelib` support); enable **Native Code** execution in Amiberry settings for the command tools and MHI library. UAESND AHI also needs the UAESND sound board enabled.
-   The command tools run on **Linux and macOS hosts** except where the Windows support table below adds support. Audio components use Amiberry's emulated interfaces; the table describes command tools only.

| Tool | Linux | macOS | Windows |
| --- | :---: | :---: | :---: |
| `host-run`, `host-multiview`, `host-shell`, `host-edit`, `host-notify` | Yes | Yes | — |
| `host-path`, `host-reveal`, `host-clip`, `host-info`, `host-download`, `host-env` | Yes | Yes | Yes |

## Exit Codes

All tools follow AmigaDOS conventions: `0` on success, `10` (`RETURN_ERROR`) when an operation fails or the tool is invoked with missing or invalid arguments, and `2` when `uae.resource` is unavailable (for example, when running outside Amiberry or with Native Code execution disabled). The explicit `?` help request returns `0`.

## Installation

1. Download the latest archive and optional `.lha.sha256` checksum from the [Releases page](../../releases).
2. Extract `Host-Tools-<version>.lha`.
3. Open the `Host-Tools` drawer and run `Install`.

All five components are selected by default. **Novice** installs everything; **Intermediate** lets you select components and copies files automatically; **Expert** lets you select components and asks before replacing existing files (showing installed and package versions where available). The installer does not edit startup files, system settings, AHI preferences, or driver configuration. The guide goes to `HELP:<language>/`, using `English` if no language is set.

For manual installation:

| Component | Copy from the package | Copy to |
| --- | --- | --- |
| Command tools | Contents of `C/` | `C:` or another directory on your command path |
| UAE AHI | `Devs/AHI/uae.audio`, `Devs/AudioModes/UAE` | `DEVS:AHI/`, `DEVS:AudioModes/` respectively |
| UAESND AHI | `Devs/AHI/uaesnd.audio`, `Devs/AudioModes/UAESND` | `DEVS:AHI/`, `DEVS:AudioModes/` respectively |
| UAE MHI | `Libs/MHI/mhiuae.library` | `LIBS:MHI/` |
| AmigaGuide | `Help/Host-Tools.guide` | `HELP:<language>/` |

Choose an AHI mode in AHI preferences after installing a driver; installing both drivers does not select one for you.

## Building

GitHub Actions builds and tests on pushes to `master`, version tags, and pull requests to `master`, using `sacredbanana/amiga-compiler:m68k-amigaos`. Tag builds also publish the release archive and its SHA-256 checksum.

With the m68k-amigaos cross-compiler, `vasmm68k_mot`, and `lha` installed locally:
```shell
make all
make test
make package
```

Alternatively, build and test with the AmigaOS 3 cross-toolchain container:
```shell
docker run --rm -v "$PWD":/work -w /work amigadev/crosstools:m68k-amigaos-gcc10 make all
docker run --rm -v "$PWD":/work -w /work amigadev/crosstools:m68k-amigaos-gcc10 make test
docker run --rm -v "$PWD":/work -w /work amigadev/crosstools:m68k-amigaos-gcc10 make package
```

On Windows, keep `tests/*.sh` and `CHANGELOG.md` checked out with LF line endings. CRLF scripts fail in `/bin/sh`, and CRLF changelog lines fail the package's release-link check.

Use `make test-unit` for native command tests without a cross-build; `make test` also runs packaging and source checks.

The `package` target verifies the staged `Host-Tools` drawer before producing `Host-Tools-<version>.lha`. The archive contains the installer, all five components listed above, the README, license, and changelog.

Release metadata is defined once in [`version.mk`](version.mk). See [`docs/RELEASING.md`](docs/RELEASING.md) for the release checklist and [`CHANGELOG.md`](CHANGELOG.md) for user-visible changes.

To build with debug output enabled:
```shell
make debug
```

## License
Copyright (C) 2020-2026 Dimitris Panokostas.

Host-Tools is licensed under the GNU General Public License version 3 or later. See [LICENSE](LICENSE) for details.
