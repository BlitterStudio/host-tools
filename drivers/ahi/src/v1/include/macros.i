TRUE EQU 1
FALSE EQU 0
NULL EQU 0

call MACRO
	jsr _LVO\1(a6)
	ENDM

push MACRO
	move.l \1,-(sp)
	ENDM

pop MACRO
	move.l (sp)+,\1
	ENDM

pushm MACRO
	movem.l \1,-(sp)
	ENDM

popm MACRO
	movem.l (sp)+,\1
	ENDM

skipw MACRO
	dc.w $0c40
	ENDM
