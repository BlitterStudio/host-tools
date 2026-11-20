<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Command tools

Run these commands in an AmigaOS CLI under Amiberry with **Native Code** enabled (`host-quit` needs only `uae.resource`). The installer copies all 12 to `C:` when you select the command tools component. Type `?` after a command for its built-in usage help.

All 12 commands work on Linux, macOS and Windows hosts. `host-shell` on Windows needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later. The others need a current Amiberry.

Where a command takes file arguments, existing Amiga paths are translated to host paths. A host application cannot normally open a file that exists only inside an unmapped hardfile.

## host-run

```text
host-run <command> [arguments...]
```

Runs a command on the host. Existing Amiga file arguments (such as `Work:Videos/clip.mp4`) are resolved to host paths. On Linux and macOS each argument is quoted for the host shell, so operators such as `|` and `>` reach the program as plain text; use `host-shell` when you need them. The command must already be installed on the host, and `host-run` cannot make files inside an unmapped Amiga volume visible to host applications.

On Linux and macOS `host-run` starts the command in the background and returns at once. The command's output goes to Amiberry's own output, not to the Amiga Shell.

On Windows the first argument is the program and the rest are its arguments. `host-run` starts the program with PowerShell's `Start-Process` and returns at once, without waiting for it to finish. It joins the arguments into one command line using the usual Windows quoting rules. A console program opens in its own window. If the program is not found, `host-run` prints a message and returns `10`.

```text
host-run vlc "Work:Videos/My Holiday.mp4"
```

For the host's default handler of a file or URL, use `host-multiview` instead of a platform-specific command.

## host-multiview

```text
host-multiview <filename|URL> [filename2|URL2 ...]
```

Opens each file or URL with the host's default application. An existing Amiga path is translated first, and a URL is passed through as a URL. On Windows the host's default handler is found with PowerShell's `Start-Process`.

```text
host-multiview https://github.com/BlitterStudio/amiberry
host-multiview "Work:Videos/My Holiday.mp4"
```

You can also set it as the DefIcons default tool for the file types you want to open on the host.

## host-shell

```text
host-shell [command]
```

On Linux and macOS, opens an interactive login shell inside the Amiga CLI, or runs the given command in that login shell, so the host shell's startup files can set up its environment. A single argument is run as written; several arguments are quoted and joined. Full-screen terminal programs work in the Amiga console. The host terminal takes its size from the Amiga window when `host-shell` starts, so restart it after resizing the window. Ctrl-C in the Amiga window is sent to the host program and does not end `host-shell`.

On Windows the session runs in a ConPTY pseudo console, which needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later. Without arguments it opens an interactive `%ComSpec%` (normally `cmd.exe`). A single argument is run through `cmd /C` as written. Several arguments are joined into one command line and quoted where needed. An argument that contains a double quote, a percent sign or a line break is refused, so pass such a command as one argument. There is no login shell on Windows. With an older Amiberry the tool reports that it could not open the session.

## host-path

```text
host-path <path> [path2 ...]
```

Prints the host path of each **existing** Amiga path. A path that does not exist, or a URL, prints an error and makes the exit code `10`. Use it to check whether a mounted directory is visible to host applications before you pass the path to `host-run`, `host-edit` or `host-multiview`.

## host-reveal

```text
host-reveal <path> [path2 ...]
```

Shows each file in the host file manager. Finder selects the file on macOS and Explorer on Windows. On Linux, file managers that support the FileManager1 D-Bus interface select it. Without such a file manager, Linux opens the containing directory. URLs are refused.

## host-notify

```text
host-notify <message>
host-notify <title> <message...>
```

Sends a desktop notification with `notify-send` on Linux, `osascript` on macOS, or a toast on Windows. With one argument the argument is the message and the title is `Amiga`. With more, the first argument is the title and the rest is the message. The Windows toast is attributed to Windows PowerShell, so it appears under that name, and title and message together are limited to about 900 characters.

## host-edit

```text
host-edit <path> [path2 ...]
```

Opens files in the host's desktop text editor. macOS uses `open -t`. On Linux the tool looks for the default GUI text editor through `xdg-mime` and `gtk-launch`, then falls back to `xdg-open`, `VISUAL` or `EDITOR` where available. On Windows the file opens in Notepad. URLs are refused.

## host-clip

```text
host-clip [copy] <text...>
host-clip copy < file
host-clip paste
```

Copies the arguments (joined with spaces) to the host clipboard, reads standard input when `copy` has no text argument, or prints the clipboard contents with `paste`. Copied text is limited to 2047 characters. Line breaks are kept on input and output, including trailing newlines. Text is converted between the Amiga's ISO-8859-1 and the host's encoding with `iconv` on Linux and macOS, or with PowerShell on Windows. Linux and macOS hosts use `pbcopy`/`pbpaste`, `wl-clipboard`, `xclip` or `xsel`, whichever is available.

```text
host-clip copy "Hello from Amiga"
host-clip paste
```

## host-info

```text
host-info
```

Prints the host operating system, the user name, and the shell, editor, opener and clipboard backend the other commands would use. Run it first when a desktop integration does not work.

## host-download

```text
host-download <URL> [<destination>] [FORCE]
```

Downloads an HTTP, HTTPS, FTP or FTPS URL with the host's `curl` or `wget` (`curl.exe` on Windows), then writes the result through AmigaDOS. This works with `RAM:`, hardfiles and directory mounts, including destinations the host cannot see.

Without a destination, the file goes in the current directory under the name from the URL. A destination that is an existing directory also keeps the URL's file name. An existing file is not overwritten unless you give `FORCE`. The tool writes to a temporary file next to the destination and moves it into place after a successful transfer, so a failed or cancelled download leaves the old file alone. Ctrl-C aborts a download. A current Amiberry streams the data and shows progress while it downloads. Older Linux and macOS builds first download to the host and then transfer the file to the Amiga.

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

Reads, sets, removes or lists host **user** environment variables. Names use ASCII letters, digits and underscores and cannot start with a digit. Values cannot contain line breaks.

On Windows, values are stored in the persistent user environment. On Linux and macOS, `set` and `unset` change `$HOME/.host-tools-env`, which is replaced atomically with mode `0600`. `get` and `list` read the host session's environment and the values from that file. Source the file from your host shell startup files to import the values into later login shells. Neither approach changes the environment of host processes that are already running, including Amiberry's parent process.

## host-quit

```text
host-quit
```

Closes Amiberry, as quitting from its menu does. Amiberry's WHDLoad booter runs it when a game exits. It works on every host, runs on a 68000, and needs only `uae.resource`, not Native Code. It waits until Amiberry closes; Ctrl-C returns.

## Exit codes and host requirements

Commands return `0` on success, `10` for missing or invalid arguments or a failed operation, and `2` if `uae.resource` is unavailable (for example outside Amiberry, or with Native Code disabled). A command that waits for a host command returns `10` if that command exits with a nonzero status. `?` returns `0` when `uae.resource` is available.

On Linux, the desktop commands need the matching host utilities: `xdg-utils`, `notify-send`, a clipboard backend, and optionally `gdbus` or `iconv`. Downloads need `curl` or `wget` on Linux and macOS and `curl.exe` on Windows. On Windows, `host-run`, `host-multiview`, `host-edit`, `host-notify`, `host-clip`, `host-info` and `host-env` run PowerShell, `host-reveal` runs Explorer, and `host-edit` needs Notepad.

`host-reveal`, `host-notify`, `host-clip`, `host-info` and `host-env` can be cancelled with Ctrl-C, and so can `host-run`, `host-multiview` and `host-edit` on Windows. These commands give up when the host sends no output for about 30 seconds, or 5 seconds on older Amiberry builds that lack `HostShell_Status`. See [Getting started](Getting-Started) for installation and host support.
