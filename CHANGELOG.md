<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

Notable user-visible changes are recorded here. This project follows semantic
versioning for release tags.

## [Unreleased]

### Added

- `host-run`, `host-multiview`, `host-edit`, `host-notify`, and `host-shell` now
  work on Windows hosts, so all 12 command tools support Linux, macOS, and
  Windows. `host-run` and `host-multiview` use PowerShell's `Start-Process`,
  `host-edit` opens Notepad, and `host-notify` shows a toast that Windows lists
  as Windows PowerShell. `host-shell` opens `%ComSpec%` in a ConPTY pseudo
  console through the new `HostShell_OpenSized` call; it needs Amiberry 8.6.0 or
  newer on Windows 10 version 1809 or later.

### Fixed

- `host-download` can no longer be sent outside the destination by a URL that
  ends in something like `OUT:file`. A file name taken from the URL must be a
  single name; otherwise the tool asks for a destination. A name cut to fit
  107 characters now produces a warning.
- `host-download` no longer saves a stray `0` at the start of a file when the
  server refuses HEAD but serves GET. The expected size now comes from the
  Content-Length header, so the size check and the progress percentage work,
  and curl no longer expands `{}` or `[]` in the URL.
- `host-download` on older Linux and macOS builds no longer gives up on a
  download that takes more than about 30 seconds.
- `host-download` on Windows passes the URL to `curl.exe` through PowerShell,
  so `%NAME%` in a URL is no longer replaced by an environment variable.
- `host-env get NAME` no longer prints the path of the environment file for
  names such as `f`, and two `host-env set` or `unset` commands that run at the
  same time no longer lose one of the changes.
- `host-clip copy` refuses standard input that contains a NUL byte or cannot be
  read, with a message of its own, instead of copying a shortened text.
- `host-notify` on Linux and macOS shows titles and messages that start with `-`
  as text instead of passing them to the notification program as options.
- `host-edit` on Linux opens files with the desktop's plain-text association, then
  `VISUAL` or `EDITOR`, and `xdg-open` last, so an image or a PDF no longer opens
  in a viewer.

### Changed

- Build with `amigadev/crosstools:m68k-amigaos` (AmigaDev GCC 16.2-rc24-9,
  binutils 2.47 snapshot), the tag that replaces `m68k-amigaos-gcc10`.
- `?` prints the usage before the tools look for `uae.resource`, so it works
  outside Amiberry. An argument after `?` is an error in every tool.
- `host-shell` converts typed text from ISO-8859-1 to UTF-8 and host output back
  to ISO-8859-1 on every host. Characters without an ISO-8859-1 form show as
  ASCII stand-ins or `?`. `host-shell ?` now explains how one argument and
  several arguments are handled.
- On Windows, a failing PowerShell command prints PowerShell's error message
  before the tool's own.

### Fixed

- `host-multiview`, `host-reveal`, `host-edit` and `host-run` no longer pass an
  Amiga path that does not exist to the host as text. `host-multiview
  HT:no-such-file` is now an error with exit code 10. `host-run` still passes
  options and other words that are not paths to the host as written.
- Volumes and assigns named like URL schemes (`DATA:`, `FILE:`, `FTP:`) are
  treated as Amiga paths when they exist. They used to be refused or sent to the
  host as URLs.
- Paths longer than 255 characters are refused before AmigaDOS sees them, and
  `host-run` no longer looks up a long ordinary argument as a path.
- `host-edit` creates a new file only in a directory that exists and has a host
  path.
- Paths and arguments with accented characters, such as `café.txt`, reach the
  host intact. Amiberry hands paths to the tools in ISO-8859-1. On Windows the
  PowerShell scripts now carry them as UTF-16, and on Linux and macOS
  `host-run`, `host-multiview`, `host-edit` and `host-reveal` convert them to
  UTF-8 before they reach the shell.
- `host-reveal` on Windows starts Explorer through PowerShell. A percent sign in
  the path is no longer expanded as an environment variable, and non-ASCII names
  no longer depend on the ANSI code page.
- `host-run` on Windows no longer lets an argument such as `x&calc.exe` start a
  second command when the target is a `.bat` or `.cmd` file. Arguments with
  quotes, percent signs or line breaks are refused for batch files.
- `host-run` and `host-edit` on Linux and macOS report a command or editor that
  cannot be started. They used to return success after handing the command to a
  background shell.

### Fixed

- `uaesnd.audio` now lets other EXTER interrupt servers run when the sound board
  has nothing pending. It used to end the interrupt chain for every interrupt.
- `uaesnd.audio` Disable and Enable preserve all registers. Disable also
  masks capture interrupts, `Update` no longer re-enables interrupts inside an
  outstanding Disable, and the interrupt server does nothing while disabled.
- `uaesnd.audio` allows one AHI allocation at a time. A second allocation fails
  instead of resetting the board under the first and releasing it on free.
- `uaesnd.audio` returns an error from Start when recording cannot begin,
  because the host has no capture device, instead of reporting success.
- `uae.audio` calls `Cause()` with ExecBase in A6. A higher priority server
  could leave another value there.
- `uae.audio` frees its per-allocation driver data when AHI frees the audio
  control, and frees the mixing and DMA buffers when Start fails, for example
  when the host cannot open an audio device.
- `uae.audio` starting recording twice no longer loses the recording buffers,
  and a Start without recording leaves a running recording alone.
- `uae.audio` stopping only recording keeps playback running, and stopping only
  playback keeps recording running, with silence sent to the host and no player
  or mixer calls. The shared host audio session now closes only when neither is
  active.
- `uae.audio` and `uaesnd.audio` free their library base and close the
  libraries they opened when initialization fails.
- The root build now also watches `drivers/mhi/src/mhi_abi.h`, so changing only
  that header rebuilds `mhiuae.library`.

## [2.9] - 2026-11-04

### Changed

- Build with GCC 16.2 from `amigadev/crosstools:m68k-amigaos-gcc10` instead of
  GCC 6.5. CI pins the image by digest.

### Fixed

- `mhiuae.library` now has its ROMTag in the first code hunk, where AmigaOS
  requires it for disk-based libraries. It used to sit in the data hunk.
- `host-download` no longer overflows on a reported size of 2 GB or more. Such
  a size is now treated as unknown, so it cannot fail the download as
  incomplete.
- `host-shell` no longer overflows when long digit runs reach its console
  window-size parser.

## [2.8] - 2026-10-26

### Fixed

- `host-quit` in the 2.7 package crashed when run: the release toolchain put
  a string constant ahead of its entry point. It now opens with a branch to
  the entry point, and the package check rejects a build that does not.

## [2.7] - 2026-10-26

### Added

- `host-quit` closes Amiberry. Amiberry's WHDLoad booter uses it when a game
  exits, in place of the third-party AmiQuit.

## [2.6] - 2026-07-22

### Added

- Publish a SHA-256 checksum beside each release archive.
- Validate every packaged command, the AmigaGuide, and `mhiuae.library` against
  the release version and date before creating an archive.
- Support native tests on both x86-64 and ARM64 hosts in the AmigaOS 3 Docker
  image.

### Changed

- Preserve trailing line breaks when pasting from POSIX host clipboards.
- Make `host-download` replacements transactional and limit downloads to HTTP,
  HTTPS, FTP, and FTPS, including curl redirects.
- Store the POSIX `host-env` file atomically with owner-only permissions and
  reject multiline values.
- Allow Ctrl-C cancellation and bound idle waits in status-aware host commands.

### Fixed

- Allow `host-env get` to read a variable without treating its absent value
  argument as invalid input, and end the displayed value with a newline.
- Filter unsupported OSC terminal control sequences from `host-shell` output
  without corrupting UTF-8 text.
- Keep the `host-shell` login wrapper within the HostShell command trap limit.
- Signal MHI buffer completion only after Amiberry has consumed a buffer.
- Close `utility.library` when UAESND AHI driver initialization fails.
- Prevent the package-layout test directory from leaking into the recursive MHI
  build.
- Propagate release metadata into binaries instead of applying the tag only to
  the archive filename.

## [2.5] - 2026-06-14

- Added `host-env` and the UAE MHI MP3 decoder library.
- Added UAESND recording support and driver refinements.

[2.9]: https://github.com/BlitterStudio/host-tools/compare/v2.8...v2.9
[2.8]: https://github.com/BlitterStudio/host-tools/compare/v2.7...v2.8
[2.7]: https://github.com/BlitterStudio/host-tools/compare/v2.6...v2.7
[2.6]: https://github.com/BlitterStudio/host-tools/compare/v2.5...v2.6
[2.5]: https://github.com/BlitterStudio/host-tools/releases/tag/v2.5
