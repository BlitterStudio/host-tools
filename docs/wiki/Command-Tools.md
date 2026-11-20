<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Command tools

Run these commands in an AmigaOS CLI under Amiberry with **Native Code** enabled (`host-quit` needs only `uae.resource`). The installer copies them all to `C:` when you select the command tools component. Type `?` after a command for its built-in usage help.

All commands work on Linux, macOS and Windows hosts. `host-shell` on Windows needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later. The others need a current Amiberry.

Where a command takes file arguments, existing Amiga paths are translated to host paths. A host application cannot normally open a file that exists only inside an unmapped hardfile.

The part of an argument before the first colon decides whether it is an Amiga path. A device, volume or assign in the DOS list makes it one, so `DATA:`, `FILE:` and `FTP:` are Amiga paths when such a volume exists, even though they look like URL schemes. If the object is missing, the tool prints `path not found` and returns `10`. It never hands the text to the host as a file name. An argument whose volume is not in the DOS list is a URL or, for `host-run`, an ordinary host argument. Windows paths such as `C:\Users\me\a.txt` are always passed through as written, even when an assign named `C:` exists. AmigaDOS paths are limited to 255 characters. A longer path is refused, and `host-run` passes a longer ordinary argument to the host unchanged without looking it up.

Amiga text and the host paths Amiberry returns are ISO-8859-1, while hosts expect UTF-8. The tools convert before they hand text or a path to the host, so a file named `café.txt` keeps its name. Names with characters outside ISO-8859-1 are not covered. On Windows the tools run PowerShell, and when it fails they print its error message before their own.

## host-run

```text
host-run <command> [arguments...]
```

Runs a command on the host. Existing Amiga file arguments (such as `Work:Videos/clip.mp4`) are resolved to host paths. On Linux and macOS each argument is quoted for the host shell, so operators such as `|` and `>` reach the program as plain text; use `host-shell` when you need them. The command must already be installed on the host, and `host-run` cannot make files inside an unmapped Amiga volume visible to host applications.

On Linux and macOS `host-run` checks that the command exists, starts it in the background and returns at once. If the command is not found, or is a file that is not executable, it prints `Command not found on host` and returns `10`. The command's input and output are discarded; nothing appears in the Amiga Shell. Amiberry builds without pipe sessions fall back to starting the command without that check.

On Windows the first argument is the program and the rest are its arguments. `host-run` starts the program with PowerShell's `Start-Process` and returns at once, without waiting for it to finish. It joins the arguments into one command line using the usual Windows quoting rules. A console program opens in its own window. If the program is not found, `host-run` prints a message and returns `10`.

Windows runs `.bat` and `.cmd` files through `cmd.exe`, which reads `&`, `|`, `<`, `>` and `^` as operators. For these files `host-run` quotes every argument that holds such a character, and it refuses an argument that contains a double quote, a percent sign or a line break. The same arguments are accepted for a program that is not a batch file. The check also covers a name without an extension that Windows resolves to a batch file on the `PATH`.

```text
host-run vlc "Work:Videos/My Holiday.mp4"
```

For the host's default handler of a file or URL, use `host-multiview` instead of a platform-specific command.

## host-multiview

```text
host-multiview <filename|URL> [filename2|URL2 ...]
```

Opens each file or URL with the host's default application. An existing Amiga path is translated first, and a URL is passed through as a URL. A path that does not exist is an error, not a host file name. On Windows the host's default handler is found with PowerShell's `Start-Process`, and a Windows path such as `C:\Users\me\a.txt` is passed through as written.

```text
host-multiview https://github.com/BlitterStudio/amiberry
host-multiview "Work:Videos/My Holiday.mp4"
```

You can also set it as the DefIcons default tool for the file types you want to open on the host.

## host-shell

```text
host-shell [command]
```

On Linux and macOS, opens an interactive login shell inside the Amiga CLI, or runs the given command in that login shell, so the host shell's startup files can set up its environment. A single argument is run as written, so it can hold a whole command line such as `"ls -l | wc -l"`. Several arguments are quoted and joined into one command, so `host-shell ls -l "My Files"` lists a file whose name has a space. Full-screen terminal programs work in the Amiga console. The host terminal takes its size from the Amiga window when `host-shell` starts, so restart it after resizing the window. Ctrl-C in the Amiga window is sent to the host program and does not end `host-shell`.

Typed text is converted from ISO-8859-1 to UTF-8, and host output is converted back, on every host. Characters that ISO-8859-1 lacks show as an ASCII stand-in (curly quotes, dashes, `...`) or `?`. The host is assumed to use UTF-8.

On Windows the session runs in a ConPTY pseudo console, which needs Amiberry 8.6.0 or newer and Windows 10 version 1809 or later. Without arguments it opens an interactive `%ComSpec%` (normally `cmd.exe`). A single argument is run through `cmd /C` as written. Several arguments are joined into one command line and quoted where needed. An argument that contains a double quote, a percent sign or a line break is refused, so pass such a command as one argument. There is no login shell on Windows. With an older Amiberry the tool reports that it could not open the session.

## host-path

```text
host-path <path> [path2 ...]
```

Prints the host path of each **existing** Amiga path. A path that does not exist, a volume that is not in the DOS list, a path longer than 255 characters or a URL prints an error and makes the exit code `10`. Use it to check whether a mounted directory is visible to host applications before you pass the path to `host-run`, `host-edit` or `host-multiview`.

## host-reveal

```text
host-reveal <path> [path2 ...]
```

Shows each file in the host file manager. Finder selects the file on macOS and Explorer on Windows, which `host-reveal` starts through PowerShell so that percent signs and non-ASCII names in the path stay intact. On Linux, file managers that support the FileManager1 D-Bus interface select it. Without such a file manager, Linux opens the containing directory. URLs are refused.

## host-notify

```text
host-notify <message>
host-notify <title> <message...>
```

Sends a desktop notification with `notify-send` on Linux, `osascript` on macOS, or a toast on Windows. With one argument the argument is the message and the title is `Amiga`. With more, the first argument is the title and the rest is the message. A title or message that starts with `-` is shown as text, not read as an option by the host utility. The Windows toast is attributed to Windows PowerShell, so it appears under that name, and title and message together are limited to about 850 characters.

## host-edit

```text
host-edit <path> [path2 ...]
```

Opens files in the host's desktop text editor. macOS uses `open -t`. On Linux the tool uses the application the desktop associates with plain text (`xdg-mime query default text/plain`, started with `gtk-launch`), then `VISUAL` or `EDITOR`, and `xdg-open` only when neither is available. The file's own type is not consulted, so a PNG or PDF opens in the text editor, not in the image viewer or PDF reader. `VISUAL` and `EDITOR` start without a terminal, so they need a graphical editor. On Windows the file opens in Notepad. URLs are refused.

A file that does not exist yet is created in its directory, so the directory must exist and have a host path: `host-edit Work:Notes/new.txt` works when `Work:Notes` exists. If no editor can be started, `host-edit` prints `Failed to edit on host` and returns `10`. A terminal editor named in `VISUAL` or `EDITOR` is started in the background without a terminal, so use `host-shell` for editors such as `vim`.

## host-clip

```text
host-clip [copy] <text...>
host-clip copy < file
host-clip paste
```

Copies the arguments (joined with spaces) to the host clipboard, reads standard input when `copy` has no text argument, or prints the clipboard contents with `paste`. Copied text is limited to 2047 characters. Standard input that holds a NUL byte, or cannot be read, is refused with its own message and the clipboard stays as it was. Line breaks are kept on input and output, including trailing newlines. Text is converted between the Amiga's ISO-8859-1 and the host's encoding with `iconv` on Linux and macOS, or with PowerShell on Windows. Linux and macOS hosts use `pbcopy`/`pbpaste`, `wl-clipboard`, `xclip` or `xsel`, whichever is available.

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

Without a destination, the file goes in the current directory under the name from the URL. A destination that is an existing directory also keeps the URL's file name. The name must be a single file name: if the end of the URL contains `:` (for example `.../OUT:file`), is `.` or `..`, or contains control characters, the tool stops and asks for an explicit destination file. A name longer than 107 characters is shortened, with a warning. An existing file is not overwritten unless you give `FORCE`. The tool writes to a temporary file next to the destination and moves it into place after a successful transfer, so a failed or cancelled download leaves the old file alone. Ctrl-C aborts a download. A current Amiberry streams the data and shows progress while it downloads. Older Linux and macOS builds first download to the host and then transfer the file to the Amiga; the host sends a space about once a second meanwhile, so a slow download is not taken for a stalled one. Downloads do not resume. curl treats `{}` and `[]` in a URL literally. On Windows the URL reaches `curl.exe` through PowerShell, so `%NAME%` in a URL is not expanded; a URL with a double quote or a control character, or longer than about 1000 characters, is refused there. The size shown comes from a HEAD request. A server that refuses HEAD still works, with progress shown in kilobytes.

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

On Windows, values are stored in the persistent user environment. On Linux and macOS, `set` and `unset` change `$HOME/.host-tools-env`, which is replaced atomically with mode `0600`. Two commands that run at the same time take turns, so neither change is lost; one that cannot get its turn within five seconds fails with a message. `get` and `list` read the host session's environment and the values from that file. Source the file from your host shell startup files to import the values into later login shells. Neither approach changes the environment of host processes that are already running, including Amiberry's parent process.

## host-quit

```text
host-quit
```

Closes Amiberry, as quitting from its menu does. Amiberry's WHDLoad booter runs it when a game exits. It works on every host, runs on a 68000, and needs only `uae.resource`, not Native Code. It waits until Amiberry closes; Ctrl-C returns.

## Exit codes and host requirements

Commands return `0` on success, `10` for missing or invalid arguments or a failed operation, and `2` if `uae.resource` is unavailable (for example outside Amiberry, or with Native Code disabled). A command that waits for a host command returns `10` if that command exits with a nonzero status. `?` prints the usage and returns `0`, even without `uae.resource`. Any argument after `?` is an error and returns `10`.

On Linux, the desktop commands need the matching host utilities: `xdg-utils`, `notify-send`, a clipboard backend, and optionally `gdbus` or `iconv`. Downloads need `curl` or `wget` on Linux and macOS and `curl.exe` on Windows. On Windows, `host-run`, `host-multiview`, `host-edit`, `host-notify`, `host-clip`, `host-info` and `host-env` run PowerShell, `host-reveal` runs Explorer, and `host-edit` needs Notepad.

`host-reveal`, `host-notify`, `host-clip`, `host-info` and `host-env` can be cancelled with Ctrl-C, and so can `host-run`, `host-multiview` and `host-edit` on Windows. These commands give up when the host sends no output for about 30 seconds, or 5 seconds on older Amiberry builds that lack `HostShell_Status`. See [Getting started](Getting-Started) for installation and host support.
