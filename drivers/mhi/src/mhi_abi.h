/*
 * SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MHI_ABI_H
#define MHI_ABI_H

#define MHIF_PLAYING 0
#define MHIF_STOPPED 1
#define MHIF_OUT_OF_DATA 2
#define MHIF_PAUSED 3

#define MHIF_UNSUPPORTED 0
#define MHIF_SUPPORTED 1
#define MHIF_FALSE 0
#define MHIF_TRUE 1

#define MHIQ_CAPABILITIES 0
#define MHIQ_MPEG1 1
#define MHIQ_MPEG2 2
#define MHIQ_MPEG25 3

#define MHIQ_LAYER3 12

#define MHIQ_VARIABLE_BITRATE 20
#define MHIQ_JOINT_STEREO 21

#define MHIQ_VOLUME_CONTROL 40
#define MHIQ_PANNING_CONTROL 41

#define MHIQ_DECODER_NAME 1000
#define MHIQ_DECODER_VERSION 1001
#define MHIQ_AUTHOR 1002

#define MHIQ_IS_HARDWARE 1010
#define MHIQ_IS_68K 1011
#define MHIQ_IS_PPC 1012

#define MHIP_VOLUME 0
#define MHIP_PANNING 1

#endif /* MHI_ABI_H */
