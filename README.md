<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Host-Tools

A classic AmigaOS 68k package for [Amiberry](https://github.com/BlitterStudio/amiberry). It has command-line tools that talk to the host operating system (Linux, macOS or Windows), two AHI audio drivers, and an MHI MP3 decoder library. The tools and drivers run inside Amiberry.

## What's included

| Component | What it does | Installed location |
| --- | --- | --- |
| Command tools | Run commands on the host, open a host shell, open or edit files and URLs, show files in the host file manager, send notifications, use the host clipboard and environment, speak text, download files, quit Amiberry | `C:` |
| UAE AHI driver | Classic UAE 16-bit stereo AHI output | `DEVS:AHI/uae.audio`, `DEVS:AudioModes/UAE` |
| UAESND AHI driver | AHI output through Amiberry's UAESND sound board | `DEVS:AHI/uaesnd.audio`, `DEVS:AudioModes/UAESND` |
| UAE MHI MP3 decoder | Offloads MP3 decoding to Amiberry | `LIBS:MHI/mhiuae.library` |
| AmigaGuide | On-Amiga reference for the tools and drivers | `HELP:<language>/Host-Tools.guide` |

The installer lets you select each component separately. Installing a driver or the MHI library does not change your audio preferences.

For details, see the [command reference](https://github.com/BlitterStudio/host-tools/wiki/Command-Tools), the [audio and MP3 guide](https://github.com/BlitterStudio/host-tools/wiki/Audio-and-MP3) and the [getting started guide](https://github.com/BlitterStudio/host-tools/wiki/Getting-Started) in the wiki. The package also has an offline AmigaGuide. The wiki pages are written in [`docs/wiki/`](docs/wiki/) next to the code and published to the GitHub wiki.

## Requirements

-   Amiberry v6.0 or newer, or a build with the `uae.resource`/`uaelib` support the tools use. Enable **Native Code** execution in the Amiberry settings for the command tools and the MHI library. `host-quit` is the exception: it needs only `uae.resource`. The UAESND AHI driver also needs the UAESND sound board enabled.
-   All command tools work on Linux, macOS and Windows hosts. The audio components use Amiberry's emulated interfaces and do not depend on the host system.
-   On Windows, `host-shell` needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later, which provides the ConPTY pseudo console. The other command tools need a current Amiberry.

## Exit codes

The tools return `0` on success and `10` (`RETURN_ERROR`) when an operation fails or an argument is missing or invalid. A tool that waits for a host command also returns `10` if that command exits with a nonzero status. They return `2` when `uae.resource` is not available, for example outside Amiberry or with Native Code disabled. Asking for usage help with `?` returns `0`, also when `uae.resource` is not available. An argument after `?` is an error and returns `10`.

## Installation

1. Download the latest archive and, if you want to verify it, the `.lha.sha256` checksum from the [Releases page](../../releases).
2. Extract `Host-Tools-<version>.lha`.
3. Open the `Host-Tools` drawer and run `Install`.

All five components are selected by default. **Novice** installs everything and replaces existing files. **Intermediate** lets you select components and copies files without asking, but leaves an installed program or library alone when it is the same version or newer. **Expert** lets you select components and asks before replacing existing files, showing the installed and package versions where available. The installer does not edit startup files, system settings, AHI preferences or driver configuration. It puts the guide in `HELP:<language>/`, using `English` if no language is set.

To install by hand:

| Component | Copy from the package | Copy to |
| --- | --- | --- |
| Command tools | Contents of `C/` | `C:` or another directory on your command path |
| UAE AHI | `Devs/AHI/uae.audio`, `Devs/AudioModes/UAE` | `DEVS:AHI/`, `DEVS:AudioModes/` respectively |
| UAESND AHI | `Devs/AHI/uaesnd.audio`, `Devs/AudioModes/UAESND` | `DEVS:AHI/`, `DEVS:AudioModes/` respectively |
| UAE MHI | `Libs/MHI/mhiuae.library` | `LIBS:MHI/` |
| AmigaGuide | `Help/Host-Tools.guide` | `HELP:<language>/` |

After installing a driver, choose its AHI mode in AHI preferences. Installing both drivers does not select either one.

## Building

GitHub Actions builds and tests on pushes to `master`, on version tags, and on pull requests to `master`. It uses `amigadev/crosstools:m68k-amigaos` (GCC 16.2), pinned by digest in [`.github/workflows/build.yml`](.github/workflows/build.yml). Tag builds also publish the release archive and its SHA-256 checksum.

With the m68k-amigaos cross-compiler, `vasmm68k_mot` and `lha` installed locally:
```shell
make all
make test
make package
```

You can also build and test in the AmigaOS 3 cross-toolchain container. The image has no native C compiler, so install one first for the unit tests:
```shell
docker run --rm -v "$PWD":/work -w /work amigadev/crosstools:m68k-amigaos sh -c \
  'apt-get update -qq && apt-get install -y -qq --no-install-recommends "gcc-$(uname -m | tr _ -)-linux-gnu" && make all test package'
```

`make test-unit` runs the native command tests without a cross-build. `make test` also runs the packaging and source checks.

The `package` target checks the staged `Host-Tools` drawer, then writes `Host-Tools-<version>.lha`. The archive contains the installer, the five components listed above, the README, the license and the changelog.

Release metadata is defined once in [`version.mk`](version.mk). See [`docs/RELEASING.md`](docs/RELEASING.md) for the release checklist and [`CHANGELOG.md`](CHANGELOG.md) for user-visible changes.

To build with debug output:
```shell
make debug
```

## License
Copyright (C) 2020-2026 Dimitris Panokostas.

Host-Tools is licensed under the GNU General Public License version 3 or later. See [LICENSE](LICENSE) for details.
