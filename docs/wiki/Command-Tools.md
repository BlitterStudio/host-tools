<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Command tools

Run these commands in an AmigaOS CLI under Amiberry with **Native Code** enabled. The package installer copies all 11 to `C:` when the command-tools component is selected. Enter `?` after a command for its built-in usage help. Existing Amiga paths are translated to host paths where a tool accepts file arguments; a host application cannot normally open a file that exists only inside an unmapped hardfile.

| Host support | Tools |
| --- | --- |
| Linux, macOS, Windows (current Amiberry) | `host-path`, `host-reveal`, `host-clip`, `host-info`, `host-download`, `host-env` |
| Linux and macOS | `host-run`, `host-multiview`, `host-shell`, `host-edit`, `host-notify` |

## host-run

```text
host-run <command> [arguments...]
```

Runs a command on a Linux or macOS host. Existing Amiga file arguments (such as `Work:Videos/clip.mp4`) are resolved to host paths. Arguments are quoted for the host shell, but the command must be installed on the host; `host-run` does not install software or make files inside an unmapped Amiga volume visible to host applications.

```text
host-run vlc "Work:Videos/My Holiday.mp4"
```

For the host's default file or URL handler, use `host-multiview` instead of choosing a platform-specific command.

## host-multiview

```text
host-multiview <filename|URL> [filename2|URL2 ...]
```

Opens each file or URL with the Linux or macOS host's default application. An existing Amiga path is translated before it is handed to the host. A URL is passed through as a URL.

```text
host-multiview https://github.com/BlitterStudio/amiberry
host-multiview "Work:Videos/My Holiday.mp4"
```

You can also set it as a DefIcons default tool for file types you want to open on the host.

## host-shell

```text
host-shell [command]
```

Opens an interactive Linux or macOS login shell inside the Amiga CLI, or runs the supplied command in that login shell. Host shell startup files can supply its environment. Full-screen terminal programs work in the Amiga console; the host terminal's size is taken from the Amiga window when `host-shell` starts. Restart it after resizing the window.

## host-path

```text
host-path <path> [path2 ...]
```

Prints the translated host-side path of each **existing** Amiga path. Use it to check whether a mounted directory is accessible to host applications before passing that path to `host-run`, `host-edit`, or `host-multiview`.

## host-reveal

```text
host-reveal <path> [path2 ...]
```

Shows translated files in the host file manager: Finder selects them on macOS, Explorer selects them on Windows, and Linux file managers supporting the FileManager1 D-Bus interface select them. On Linux without a compatible file manager, the containing directory is opened instead.

## host-notify

```text
host-notify <message>
host-notify <title> <message...>
```

Sends a desktop notification using `notify-send` on Linux or `osascript` on macOS. The one-argument form uses the message as the notification text; the second form supplies a title and message. Windows is not supported.

## host-edit

```text
host-edit <path> [path2 ...]
```

Opens files in the Linux or macOS host's desktop text editor. macOS uses `open -t`; on Linux the tool looks for a default GUI text editor through `xdg-mime` and `gtk-launch`, then falls back to `xdg-open`, `VISUAL`, or `EDITOR` where available.

## host-clip

```text
host-clip [copy] <text...>
host-clip copy < file
host-clip paste
```

Copies arguments to the host clipboard, reads standard input when `copy` has no text argument, or prints the clipboard contents with `paste`. Input and output preserve line breaks, including trailing newlines. Text converts between the Amiga's ISO-8859-1 encoding and the host's encoding via `iconv` on Linux/macOS or PowerShell on Windows. POSIX hosts use an available `wl-clipboard`, `xclip`, or `xsel` backend.

```text
host-clip copy "Hello from Amiga"
host-clip paste
```

## host-info

```text
host-info
```

Prints the detected host operating system and the available shell, opener, editor, and clipboard backend. Run this before troubleshooting a missing desktop integration.

## host-download

```text
host-download <URL> [<destination>] [FORCE]
```

Downloads an HTTP, HTTPS, FTP, or FTPS URL through the host's `curl` or `wget` (`curl.exe` on Windows), then writes the result through AmigaDOS. This works with `RAM:`, hardfiles, and directory mounts, including destinations the host cannot see directly.

Without a destination, the URL filename is used in the current directory. An existing directory destination also keeps the URL filename. An existing file is not overwritten unless `FORCE` is specified. The tool writes to a temporary file beside the destination and moves it into place after a successful transfer; failed or cancelled transfers do not replace the old file. Ctrl-C aborts a download. A current Amiberry can stream data with live progress; older Linux/macOS builds first download to the host and then transfer to the Amiga.

```text
host-download https://github.com/BlitterStudio/host-tools/releases/download/v2.6/Host-Tools-2.6.lha RAM:
```

## host-env

```text
host-env get NAME
host-env set NAME VALUE
host-env unset NAME
host-env list
```

Reads, sets, removes, or lists **host user** environment variables. Names use ASCII letters, digits, and underscores and cannot start with a digit. Values cannot contain line breaks.

On Windows, values are stored in the persistent user environment. On Linux/macOS, changes are written atomically to `$HOME/.host-tools-env` with mode `0600`; source that file from your host shell startup files to import values into later login shells. Neither approach changes the environment of host processes that are already running, including Amiberry's parent process.

## Exit codes and host requirements

Commands return `0` on success, `10` for missing/invalid arguments or failed operations, and `2` if `uae.resource` is unavailable (for example, outside Amiberry or with Native Code disabled). The `?` help request returns `0` when `uae.resource` is available.

Linux desktop commands depend on the corresponding host utilities (`xdg-utils`, `notify-send`, clipboard backends, and optionally `gdbus` or `iconv`). Downloads require `curl` or `wget` on POSIX hosts and `curl.exe` on Windows. Status-aware host commands can be cancelled with Ctrl-C; idle operations time out after about 30 seconds with newer Amiberry `HostShell_Status` support or 5 seconds without it. See [Getting started](Getting-Started) for installation and host support.
