<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Releasing Host-Tools

Release metadata lives in `version.mk`. The tag must be `v` followed by that
exact version; CI rejects a mismatch before building.

## Checklist

1. Update `VERSION` and `DATE` in `version.mk`.
2. Update the `$VER` line in `package/Help/Host-Tools.guide`.
3. Move the pending changelog entry from `Unreleased` to the release date.
4. Run the complete release build from a clean tree:

   ```shell
   docker run --rm -v "$PWD":/work -w /work \
     sacredbanana/amiga-compiler:m68k-amigaos make clean test package
   ```

5. Confirm that `git status --short` contains only intentional source changes.
   `make package` already verifies the package layout and all release version
   strings before writing the archive.
6. Review the archive and checksum locally if desired:

   ```shell
   lha l "Host-Tools-$(make -s print-version).lha"
   shasum -a 256 "Host-Tools-$(make -s print-version).lha"
   ```

7. Merge the release pull request, then create and push the matching annotated
   tag:

   ```shell
   version="$(make -s print-version)"
   git tag -a "v$version" -m "Host-Tools v$version"
   git push origin "v$version"
   ```

The tag workflow rebuilds and verifies the package, publishes the `.lha` and
`.lha.sha256` artifacts, and creates the GitHub release with generated notes.
