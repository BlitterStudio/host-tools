<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

Notable user-visible changes are recorded here. This project follows semantic
versioning for release tags.

## [2.10] - 2026-11-20

### Added

- `host-copy` copies one file between any host path and the Amiga, including
  host folders that are not mounted as Amiga volumes:
  `host-copy get <host path> <Amiga destination>` and
  `host-copy put <Amiga source> <host path>`. It checks the byte count before
  it replaces the destination, and it refuses to overwrite an existing file
  unless you give `FORCE`.
- `host-say` speaks text on the host and returns when the speech ends. It reads
  standard input when given no text, and Ctrl-C stops the speech. It uses `say`
  on macOS, `spd-say`, `espeak-ng` or `espeak` on Linux, and `System.Speech` on
  Windows.
- `host-config` reads and changes Amiberry settings while Amiberry runs, for
  example `host-config get cpu_speed` and `host-config set cpu_speed max`. A
  value that names an existing Amiga file is passed on as its host path.
- `host-disk` lists the floppy drives DF0: to DF3: with their disk images, and
  inserts or ejects images: `host-disk insert DF0: Work:Disks/Game.adf`. It
  reports an error when the drive is not in the configuration or Amiberry
  cannot load the image.
- `host-config` and `host-disk` talk to Amiberry through `uae.resource` and run
  no host commands, so they work on every host and without Native Code.
- `host-run`, `host-multiview`, `host-edit`, `host-notify` and `host-shell` now
  work on Windows hosts, so every command tool supports Linux, macOS and
  Windows. `host-run` and `host-multiview` use PowerShell's `Start-Process`,
  `host-edit` opens Notepad, and `host-notify` shows a toast listed under
  Windows PowerShell. `host-shell` opens `%ComSpec%` in a ConPTY pseudo
  console. It needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or
  later.

### Changed

- `?` prints the usage before a tool looks for `uae.resource`, so it also works
  outside Amiberry. An argument after `?` is an error in every tool.
- `host-shell` converts typed text from ISO-8859-1 to UTF-8, and host output
  back to ISO-8859-1, on every host. Characters with no ISO-8859-1 form show as
  ASCII stand-ins or `?`.
- On Windows, a failing host command prints PowerShell's error message before
  the tool's own.
- `host-run` and `host-edit` on Linux and macOS start the program through a host
  command session, so they can report a program or editor that cannot be
  started. They used to report success in every case.
- The README, wiki and AmigaGuide are rewritten in plainer language, and several
  wrong statements are corrected. The AmigaGuide gains a page on exit codes.
- Build with `amigadev/crosstools:m68k-amigaos` (AmigaDev GCC 16.2-rc24-9,
  binutils 2.47 snapshot), the tag that replaces `m68k-amigaos-gcc10`.

### Fixed

- Command tools:
  - Paths and arguments with accented characters, such as `café.txt`, reach
    the host intact. Linux and macOS used to receive them in ISO-8859-1.
  - An Amiga path that does not exist is an error instead of being passed to
    the host as text. `host-multiview HT:no-such-file` used to open a "Pick an
    app" dialog on Windows.
  - Volumes and assigns named like URL schemes (`DATA:`, `FILE:`, `FTP:`) are
    treated as Amiga paths when they exist.
  - Paths longer than 255 characters are refused before AmigaDOS sees them.
  - `host-edit` creates a new file only in a directory that exists and has a
    host path.
- `host-download`:
  - A URL that ends in something like `OUT:file` can no longer write outside
    the destination. The file name taken from a URL must be a single name.
  - The tool warns when it shortens a long file name to fit 107 characters.
  - A server that refuses HEAD but serves GET no longer leaves a stray `0` at
    the start of the file.
  - The expected size is now read from the Content-Length header, so the size
    check and the progress percentage work.
  - curl no longer expands `{}` or `[]` in the URL.
  - Older Linux and macOS builds no longer give up on a download that takes
    more than about 30 seconds.
  - On Windows, `%NAME%` in a URL is no longer replaced by an environment
    variable.
- `host-env`:
  - `host-env get f` and similar short names print the variable instead of the
    path of the environment file.
  - Two `set` or `unset` commands that run at the same time no longer lose one
    of the changes.
- `host-clip copy` refuses standard input that contains a NUL byte or cannot be
  read, instead of copying a shortened text.
- `host-notify` on Linux and macOS shows titles and messages that start with
  `-` as text, instead of passing them to the notification program as options.
- `host-edit` on Linux opens files with the desktop's plain-text editor, then
  `VISUAL` or `EDITOR`, and `xdg-open` last, so an image or a PDF no longer
  opens in a viewer.
- `host-reveal` on Windows starts Explorer through PowerShell, so a percent sign
  in a path is no longer expanded as an environment variable.
- `host-run` on Windows no longer lets an argument such as `x&calc.exe` start a
  second command when the target is a `.bat` or `.cmd` file. Arguments with
  quotes, percent signs or line breaks are refused for batch files.
- `uaesnd.audio`:
  - It lets other EXTER interrupt servers run when the sound board has nothing
    pending. It used to end the interrupt chain for every interrupt.
  - Disable and Enable preserve all registers.
  - Disable also masks capture interrupts, and Update no longer re-enables
    interrupts inside an outstanding Disable.
  - It allows one AHI allocation at a time. A second allocation used to reset
    the board under the first one.
  - Start returns an error when recording cannot begin, for example when the
    host has no capture device.
- `uae.audio`:
  - It calls `Cause()` with ExecBase in A6.
  - It no longer leaks memory on every allocate and free, or when Start fails.
  - Starting recording twice no longer loses the recording buffers.
  - Stopping only recording keeps playback running, and stopping only playback
    keeps recording running.
- Both AHI drivers free their library base and close the libraries they opened
  when initialisation fails.
- Changing `drivers/mhi/src/mhi_abi.h` alone now rebuilds `mhiuae.library`.

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

[2.10]: https://github.com/BlitterStudio/host-tools/compare/v2.9...v2.10
[2.9]: https://github.com/BlitterStudio/host-tools/compare/v2.8...v2.9
[2.8]: https://github.com/BlitterStudio/host-tools/compare/v2.7...v2.8
[2.7]: https://github.com/BlitterStudio/host-tools/compare/v2.6...v2.7
[2.6]: https://github.com/BlitterStudio/host-tools/compare/v2.5...v2.6
[2.5]: https://github.com/BlitterStudio/host-tools/releases/tag/v2.5
