entropy_get := $FECF
mouse_config := $FF68
mouse_get := $FF6B
screen_mode := $FF5F

.struct mouse
	x_pos .word
	y_pos .word
	buttons .byte
.endstruct

.setcpu "65c02"
.importzp tmp1, tmp2, tmp3, tmp4
.importzp ptr1, ptr2, ptr3, ptr4
.importzp sreg

DEFAULT_COLOR := $61

.export _deck
_deck := $9000

.export _facedown_piles
_facedown_piles := $9080

.export _faceup_piles
_faceup_piles := $9100

RDTIM = $FFDE

.importzp ptr1

.export _waitforjiffy
_waitforjiffy:
jsr RDTIM
sta ptr1
:
jsr RDTIM
cmp ptr1
beq :-

rts

.export _rand_byte
_rand_byte:
	jsr entropy_get
	stx tmp1
	eor tmp1
	sty tmp1
	eor tmp1
	rts

.export _rand_word
_rand_word:
	jsr _rand_byte
	pha
	jsr _rand_byte
	plx
	rts
	
SCREEN_HEIGHT = 60
SCREEN_WIDTH = 80

.export _clear_screen
_clear_screen:
	stz $9F20
	stz $9F21
	
	lda #$10
	sta $9F22
	
	ldy #0
	:
	ldx #0
	:
	lda #$20
	sta $9F23
	lda #DEFAULT_COLOR
	sta $9F23
	
	inx
	cpx #SCREEN_WIDTH
	bcc :-
	
	inc $9F21
	stz $9F20
	
	iny
	cpy #SCREEN_HEIGHT
	bcc :--
	rts

.export _enable_mouse
_enable_mouse:
	sec
	jsr screen_mode
	lda #1
	jmp mouse_config
	
.export _mouse_get
_mouse_get:
	sta ptr1
	stx ptr1 + 1
	
	ldx #$02
	jsr mouse_get
	ldy #mouse::buttons
	sta (ptr1), Y
	dey
	:
	lda $02, Y
	sta (ptr1), Y
	dey
	bpl :-
	rts
