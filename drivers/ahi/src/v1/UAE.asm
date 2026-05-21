; SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
; SPDX-License-Identifier: GPL-3.0-or-later

TRUE            EQU     1
FALSE           EQU     0

AHI_TagBase     EQU     $80000000
AHI_TagBaseR    EQU     AHI_TagBase+$8000
AHIDB_AudioID   EQU     AHI_TagBase+100
AHIDB_Volume    EQU     AHI_TagBase+103
AHIDB_Panning   EQU     AHI_TagBase+104
AHIDB_Stereo    EQU     AHI_TagBase+105
AHIDB_HiFi      EQU     AHI_TagBase+106
AHIDB_MultTable EQU     AHI_TagBase+108
AHIDB_Name      EQU     AHI_TagBaseR+109
AHIDB_Bits      EQU     AHI_TagBase+110
TAG_DONE        EQU     0

BEG:
        dc.b    "FORM"
        dc.l    E-S
S:
        dc.b    "AHIM"

        dc.b    "AUDN"
        dc.l    .audn_e-.audn_s
.audn_s:
        dc.b    "uae",0
.audn_e:
        CNOP    0,2

        dc.b    "AUDM"
        dc.l    .audm_e-.audm_s
.audm_s:
        dc.l    AHIDB_AudioID,$001a0000
        dc.l    AHIDB_Bits,16
        dc.l    AHIDB_Volume,TRUE
        dc.l    AHIDB_Panning,TRUE
        dc.l    AHIDB_Stereo,TRUE
        dc.l    AHIDB_HiFi,TRUE
        dc.l    AHIDB_MultTable,FALSE
        dc.l    AHIDB_Name,.name-.audm_s
        dc.l    TAG_DONE
        dc.l    TAG_DONE
.name:
        dc.b    "UAE :16 bit HIFI Stereo++",0
.audm_e:
        CNOP    0,2
E:
