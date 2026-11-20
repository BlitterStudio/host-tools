<!--
SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Audio drivers and MP3 decoding

Host-Tools has two separate AHI drivers and an MHI MP3 decoder library for classic AmigaOS 68k under Amiberry. The installer selects all three by default, and you can install any combination. It only copies files, so choose an AHI mode in AHI preferences yourself. The MHI decoder needs no AHI mode of its own.

| Component | Package files | Installed location |
| --- | --- | --- |
| UAE AHI | `Devs/AHI/uae.audio`, `Devs/AudioModes/UAE` | `DEVS:AHI/`, `DEVS:AudioModes/` |
| UAESND AHI | `Devs/AHI/uaesnd.audio`, `Devs/AudioModes/UAESND` | `DEVS:AHI/`, `DEVS:AudioModes/` |
| UAE MHI MP3 | `Libs/MHI/mhiuae.library` | `LIBS:MHI/` |

## UAE AHI driver

The `UAE :16 bit HIFI Stereo++` AHI mode plays 16-bit stereo audio with panning through Amiberry's classic UAE AHI v1 host interface. Install both `uae.audio` and its `UAE` AudioMode file, then select that mode in AHI preferences. Installing does not switch the active AHI output.

## UAESND AHI driver

Enable the **UAESND sound board** in Amiberry before you select a `uaesnd` mode in AHI preferences. The driver sends each AHI channel to a hardware audio stream, so there is no Amiga-side mixing. It provides these modes:

| Mode shown in AHI | Output |
| --- | --- |
| `uaesnd: Stereo` | 16-bit stereo |
| `uaesnd: HiFi Stereo` | 32-bit stereo |
| `uaesnd: 7.1` | 32-bit multichannel |

Install both `uaesnd.audio` and its `UAESND` AudioMode file. The installer does not enable the sound board or change your AHI preferences.

## UAE MHI MP3 decoder

`mhiuae.library` implements the classic MHI decoder interface for MHI-compatible MP3 players. It passes MP3 buffers to Amiberry through `uae.resource`. Amiberry decodes them with its host mpg123 decoder and feeds the audio into the emulator's mixer, so decoding takes no Amiga CPU time.

Install the library in `LIBS:MHI/`. It does not configure AHI, and it works with Paula, UAE AHI, UAESND AHI or any other AHI driver you choose. Enable **Native Code** execution in Amiberry for the `uae.resource` integration.

See [Getting started](Getting-Started) for the installer and host requirements, and [Command tools](Command-Tools) for the other programs in the package.
