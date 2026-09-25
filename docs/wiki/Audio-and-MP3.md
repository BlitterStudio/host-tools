<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Audio drivers and MP3 decoding

Host-Tools includes two separate AHI drivers and an MHI MP3 decoder library for classic AmigaOS 68k under Amiberry. The installer selects all three by default but lets you install any combination. It copies files only: **you must choose an AHI mode in AHI preferences** yourself. The MHI decoder does not need an AHI mode of its own.

| Component | Package files | Installed location |
| --- | --- | --- |
| UAE AHI | `Devs/AHI/uae.audio`, `Devs/AudioModes/UAE` | `DEVS:AHI/`, `DEVS:AudioModes/` |
| UAESND AHI | `Devs/AHI/uaesnd.audio`, `Devs/AudioModes/UAESND` | `DEVS:AHI/`, `DEVS:AudioModes/` |
| UAE MHI MP3 | `Libs/MHI/mhiuae.library` | `LIBS:MHI/` |

## UAE AHI driver

The `UAE :16 bit HIFI Stereo++` AHI mode outputs 16-bit stereo audio with panning through Amiberry's classic UAE AHI v1 host interface. Install both `uae.audio` and its `UAE` AudioMode file, then select that mode in AHI preferences to use it. Installation alone does not switch the active AHI output.

## UAESND AHI driver

Enable the **UAESND sound board** in Amiberry before selecting a `uaesnd` mode in AHI preferences. This driver sends each AHI channel to a hardware audio stream, avoiding Amiga-side mixing. It provides:

| Mode shown in AHI | Output |
| --- | --- |
| `uaesnd: Stereo` | 16-bit stereo |
| `uaesnd: HiFi Stereo` | 32-bit stereo |
| `uaesnd: 7.1` | 32-bit multichannel |

Install both `uaesnd.audio` and its `UAESND` AudioMode file. The installer neither enables the sound board nor changes your AHI preferences.

## UAE MHI MP3 decoder

`mhiuae.library` implements the classic MHI decoder interface for MHI-compatible MP3 players. It passes MP3 buffers through `uae.resource`; Amiberry uses its host mpg123 decoder and feeds the resulting audio into the emulator's audio mixer. MP3 decoding therefore does not consume Amiga CPU time.

Install the library in `LIBS:MHI/`. It does not configure AHI and can be used with Paula, UAE AHI, UAESND AHI, or another chosen AHI driver. Enable **Native Code** execution in Amiberry for the `uae.resource` integration.

Return to [Getting started](Getting-Started) for the installer and host requirements, or to [Command tools](Command-Tools) for the other programs in the package.
