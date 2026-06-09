	incdir	sys:programming/asm/includes/
	include devices/ahi.i
	include libraries/ahi_sub.i
start:	
	dc.l	ID_FORM
	dc.l	E-S
S
	dc.l	ID_AHIM

	dc.l	ID_AUDN
	dc.l	ex-sx
sx
	dc.b	"uaesnd",0
ex
	cnop	0,2

	dc.l	ID_AUDM
	dc.l	e12-s12
s12
	dc.l	AHIDB_AudioID,$003b0001
	dc.l	AHIDB_Bits,16
	dc.l	AHIDB_Volume,1
	dc.l	AHIDB_Panning,0
	dc.l	AHIDB_Stereo,1
	dc.l	AHIDB_Name,name1-s12
	dc.l	TAG_DONE
name1
	dc.b	"uaesnd: Stereo",0
e12
	cnop 0,2

	dc.l	ID_AUDM
	dc.l	e32-s32
s32
	dc.l	AHIDB_AudioID,$003b0002
	dc.l	AHIDB_Bits,32
	dc.l	AHIDB_Volume,1
	dc.l	AHIDB_Panning,0
	dc.l	AHIDB_Stereo,1
	dc.l	AHIDB_HiFi,1
	dc.l	AHIDB_Name,name3-s32
	dc.l	TAG_DONE
name3
	dc.b	"uaesnd: HiFi Stereo",0
e32
	cnop 0,2

	dc.l	ID_AUDM
	dc.l	e22-s22
s22
	dc.l	AHIDB_AudioID,$003b0003
	dc.l	AHIDB_Bits,32
	dc.l	AHIDB_Volume,1
	dc.l	AHIDB_Panning,0
	dc.l	AHIDB_Stereo,1
	dc.l	AHIDB_HiFi,1
	dc.l	AHIDB_MultiChannel,1
	dc.l	AHIDB_Name,name2-s22
	dc.l	TAG_DONE
name2
	dc.b	"uaesnd: 7.1",0
e22
	cnop 0,2

E
	cnop 0,2
end:
