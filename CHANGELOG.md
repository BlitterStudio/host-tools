<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Changelog

Notable user-visible changes are recorded here. This project follows semantic
versioning for release tags.

## [2.6] - Unreleased

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

[2.6]: https://github.com/BlitterStudio/host-tools/compare/v2.5...HEAD
[2.5]: https://github.com/BlitterStudio/host-tools/releases/tag/v2.5
