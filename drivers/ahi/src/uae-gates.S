| SPDX-FileCopyrightText: 2020-2026 Dimitris Panokostas
| SPDX-License-Identifier: GPL-3.0-or-later

	.text

	.globl	_gwLibInit
_gwLibInit:
	move.l	a6,-(sp)
	move.l	a0,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_LibInit
	lea	12(sp),sp
	rts

	.globl	_gwLibOpen
_gwLibOpen:
	move.l	a6,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_LibOpen
	lea	8(sp),sp
	rts

	.globl	_gwLibClose
_gwLibClose:
	move.l	a6,-(sp)
	jsr	_UAE_LibClose
	addq.l	#4,sp
	rts

	.globl	_gwLibExpunge
_gwLibExpunge:
	move.l	a6,-(sp)
	jsr	_UAE_LibExpunge
	addq.l	#4,sp
	rts

	.globl	_gwLibNull
_gwLibNull:
	move.l	a6,-(sp)
	jsr	_UAE_LibNull
	addq.l	#4,sp
	rts

	.globl	_gwAHIsub_AllocAudio
_gwAHIsub_AllocAudio:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	a1,-(sp)
	jsr	_UAE_AHIsub_AllocAudio
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_FreeAudio
_gwAHIsub_FreeAudio:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	jsr	_UAE_AHIsub_FreeAudio
	lea	8(sp),sp
	rts

	.globl	_gwAHIsub_Disable
_gwAHIsub_Disable:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	jsr	_UAE_AHIsub_Disable
	lea	8(sp),sp
	rts

	.globl	_gwAHIsub_Enable
_gwAHIsub_Enable:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	jsr	_UAE_AHIsub_Enable
	lea	8(sp),sp
	rts

	.globl	_gwAHIsub_Start
_gwAHIsub_Start:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_Start
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_Update
_gwAHIsub_Update:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_Update
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_Stop
_gwAHIsub_Stop:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_Stop
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_SetVol
_gwAHIsub_SetVol:
	move.l	a6,-(sp)
	move.l	d3,-(sp)
	move.l	a2,-(sp)
	move.l	d2,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_SetVol
	lea	24(sp),sp
	rts

	.globl	_gwAHIsub_SetFreq
_gwAHIsub_SetFreq:
	move.l	a6,-(sp)
	move.l	d2,-(sp)
	move.l	a2,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_SetFreq
	lea	20(sp),sp
	rts

	.globl	_gwAHIsub_SetSound
_gwAHIsub_SetSound:
	move.l	a6,-(sp)
	move.l	d4,-(sp)
	move.l	a2,-(sp)
	move.l	d3,-(sp)
	move.l	d2,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_SetSound
	lea	28(sp),sp
	rts

	.globl	_gwAHIsub_SetEffect
_gwAHIsub_SetEffect:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	a0,-(sp)
	jsr	_UAE_AHIsub_SetEffect
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_LoadSound
_gwAHIsub_LoadSound:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	a0,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_LoadSound
	lea	20(sp),sp
	rts

	.globl	_gwAHIsub_UnloadSound
_gwAHIsub_UnloadSound:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_UnloadSound
	lea	12(sp),sp
	rts

	.globl	_gwAHIsub_GetAttr
_gwAHIsub_GetAttr:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	a1,-(sp)
	move.l	d2,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_GetAttr
	lea	24(sp),sp
	rts

	.globl	_gwAHIsub_HardwareControl
_gwAHIsub_HardwareControl:
	move.l	a6,-(sp)
	move.l	a2,-(sp)
	move.l	d1,-(sp)
	move.l	d0,-(sp)
	jsr	_UAE_AHIsub_HardwareControl
	lea	16(sp),sp
	rts

	.globl	_ahi_call_pre_timer
_ahi_call_pre_timer:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a0
	move.l	52(sp),a2
	jsr	(a0)
	movem.l	(sp)+,d2-d7/a2-a6
	rts

	.globl	_ahi_call_post_timer
_ahi_call_post_timer:
	movem.l	d2-d7/a2-a6,-(sp)
	move.l	48(sp),a0
	move.l	52(sp),a2
	jsr	(a0)
	movem.l	(sp)+,d2-d7/a2-a6
	rts

	.globl	_ahi_interrupt_entry
_ahi_interrupt_entry:
	movem.l	d0-d7/a0-a6,-(sp)
	move.l	a1,-(sp)
	jsr	_UAE_AHIInterrupt
	addq.l	#4,sp
	movem.l	(sp)+,d0-d7/a0-a6
	moveq	#0,d0
	rts

	.globl	_ahi_softint_entry
_ahi_softint_entry:
	movem.l	d0-d7/a0-a6,-(sp)
	move.l	a1,-(sp)
	jsr	_UAE_AHISoftInt
	addq.l	#4,sp
	movem.l	(sp)+,d0-d7/a0-a6
	rts

	.globl	_uae_find_trap
_uae_find_trap:
	move.l	4(sp),a1
	move.l	8(sp),a0
	subq.l	#4,a1
	move.l	(a1),a1
	jsr	(a1)
	rts

	.globl	_uae_ahi_open
_uae_ahi_open:
	movem.l	d2-d5,-(sp)
	move.l	20(sp),a1
	move.l	24(sp),d1
	move.l	28(sp),d2
	move.l	32(sp),d3
	move.l	36(sp),d4
	move.l	40(sp),d5
	moveq	#6,d0
	jsr	(a1)
	movem.l	(sp)+,d2-d5
	rts

	.globl	_uae_ahi_close
_uae_ahi_close:
	move.l	4(sp),a1
	move.l	8(sp),d1
	moveq	#1,d0
	jsr	(a1)
	rts

	.globl	_uae_ahi_write
_uae_ahi_write:
	move.l	4(sp),a1
	move.l	8(sp),d1
	move.l	12(sp),a0
	moveq	#2,d0
	jsr	(a1)
	rts

	.globl	_uae_ahi_write_interrupt
_uae_ahi_write_interrupt:
	move.l	4(sp),a1
	move.l	8(sp),d1
	moveq	#4,d0
	jsr	(a1)
	rts

	.globl	_uae_ahi_poll
_uae_ahi_poll:
	move.l	4(sp),a1
	moveq	#5,d0
	jsr	(a1)
	rts
