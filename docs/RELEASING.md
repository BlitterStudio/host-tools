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
3. Move the pending changelog entry from `Unreleased` to the release date and
   update its comparison link to end at the release tag instead of `HEAD`.
4. Run the complete release build from a clean tree:

   ```shell
   docker run --rm -v "$PWD":/work -w /work \
     amigadev/crosstools:m68k-amigaos-gcc10 make clean test package
   ```

   On Windows, check out `tests/*.sh` and `CHANGELOG.md` with LF line endings;
   CRLF breaks the shell tests and the changelog link check inside Docker.

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

## Publishing the wiki

The detailed wiki pages are authored in `docs/wiki/`, not in the wiki editor.
Publish them when their content changes, after the corresponding repository
change is merged; keep the release archive's README and AmigaGuide usable
offline. The GitHub wiki is a separate Git repository, so merging this
repository does not update the live wiki:

```shell
gh repo clone BlitterStudio/host-tools.wiki ../host-tools.wiki
cp docs/wiki/*.md ../host-tools.wiki/
git -C ../host-tools.wiki add Home.md Getting-Started.md Command-Tools.md Audio-and-MP3.md
git -C ../host-tools.wiki commit -m "Update Host-Tools user guides"
git -C ../host-tools.wiki push
```

Clone only once; for subsequent updates, pull the wiki's latest commit before
copying the pages. The first page of a new wiki must be created in GitHub's
wiki editor before its Git repository can be cloned.
