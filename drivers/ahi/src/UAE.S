| SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
| SPDX-License-Identifier: GPL-3.0-or-later

	.macro	LONG num
	.long	\num
	.endm

	.macro	LONG2 num1,num2
	.long	\num1,\num2
	.endm

	.macro	FORM_START name
	.ascii	"FORM"
	LONG	FORMEND-FORMSTART
FORMSTART:
	.ascii	"\name"
	.endm

	.macro	FORM_END
FORMEND:
	.balign	2,0
	.endm

	.macro	CHUNK_START name
	.ascii	"\name"
	LONG	101f-100f
100:
	.endm

	.macro	CHUNK_END
101:
	.balign	2,0
	.endm

	.set	AHI_TagBase,		2147483648
	.set	AHI_TagBaseR,		AHI_TagBase+32768
	.set	AHIDB_AudioID,		AHI_TagBase+100
	.set	AHIDB_Volume,		AHI_TagBase+103
	.set	AHIDB_Panning,		AHI_TagBase+104
	.set	AHIDB_Stereo,		AHI_TagBase+105
	.set	AHIDB_HiFi,		AHI_TagBase+106
	.set	AHIDB_PingPong,		AHI_TagBase+107
	.set	AHIDB_MultTable,	AHI_TagBase+108
	.set	AHIDB_Name,		AHI_TagBaseR+109
	.set	TAG_DONE,		0
	.set	TRUE,			1
	.set	FALSE,			0

	FORM_START	AHIM

	CHUNK_START	AUDN
	.asciz		"uae"
	CHUNK_END

	CHUNK_START	AUDM
1:
	LONG2		AHIDB_AudioID,	0x001a0001
	LONG2		AHIDB_Volume,	TRUE
	LONG2		AHIDB_Panning,	TRUE
	LONG2		AHIDB_Stereo,	TRUE
	LONG2		AHIDB_HiFi,	TRUE
	LONG2		AHIDB_PingPong,	FALSE
	LONG2		AHIDB_MultTable,FALSE
	LONG2		AHIDB_Name,	2f-1b
	LONG		TAG_DONE
2:
	.asciz		"UAE: HiFi stereo++"
	CHUNK_END

	CHUNK_START	AUDM
1:
	LONG2		AHIDB_AudioID,	0x001a0002
	LONG2		AHIDB_Volume,	TRUE
	LONG2		AHIDB_Panning,	FALSE
	LONG2		AHIDB_Stereo,	TRUE
	LONG2		AHIDB_HiFi,	TRUE
	LONG2		AHIDB_PingPong,	FALSE
	LONG2		AHIDB_MultTable,FALSE
	LONG2		AHIDB_Name,	2f-1b
	LONG		TAG_DONE
2:
	.asciz		"UAE: HiFi stereo"
	CHUNK_END

	FORM_END
