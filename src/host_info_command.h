/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HOST_INFO_COMMAND_H
#define HOST_INFO_COMMAND_H

#define HOST_INFO_COMMAND \
    "printf 'OS: '; uname -srm 2>/dev/null || printf 'unknown\\n'; " \
    "printf 'User: '; id -un 2>/dev/null || whoami 2>/dev/null || printf 'unknown\\n'; " \
    "printf 'Shell: %s\\n' \"${SHELL:-unknown}\"; " \
    "printf 'Editor: '; " \
    "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then echo 'open -t'; " \
    "elif command -v xdg-mime >/dev/null 2>&1 && command -v gtk-launch >/dev/null 2>&1; then desktop_file=$(xdg-mime query default text/plain 2>/dev/null); " \
    "if [ -n \"$desktop_file\" ]; then printf 'gtk-launch %s\\n' \"$desktop_file\"; " \
    "elif command -v xdg-open >/dev/null 2>&1; then echo xdg-open; " \
    "elif [ -n \"${VISUAL:-${EDITOR:-}}\" ]; then printf '%s\\n' \"${VISUAL:-${EDITOR:-}}\"; else echo unavailable; fi; " \
    "elif command -v xdg-open >/dev/null 2>&1; then echo xdg-open; " \
    "elif [ -n \"${VISUAL:-${EDITOR:-}}\" ]; then printf '%s\\n' \"${VISUAL:-${EDITOR:-}}\"; else echo unavailable; fi; " \
    "printf 'Opener: '; " \
    "if [ \"$(uname -s)\" = Darwin ] && command -v open >/dev/null 2>&1; then echo open; " \
    "elif command -v xdg-open >/dev/null 2>&1; then echo xdg-open; else echo unavailable; fi; " \
    "printf 'Clipboard: '; " \
    "if command -v pbcopy >/dev/null 2>&1 && command -v pbpaste >/dev/null 2>&1; then echo pbcopy/pbpaste; " \
    "elif command -v wl-copy >/dev/null 2>&1 && command -v wl-paste >/dev/null 2>&1; then echo wl-clipboard; " \
    "elif command -v xclip >/dev/null 2>&1; then echo xclip; " \
    "elif command -v xsel >/dev/null 2>&1; then echo xsel; else echo unavailable; fi"

#endif
