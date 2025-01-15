.ifndef MOVEMENT_INC
MOVEMENT_INC = 1

.include "global.s"

.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfParam1

.segment "BANKRAM0A"

;bAMoveDirection Inputs (a (local position (x coord or y coord not both))/sreg ego position) Returns a/x (distance/direction)
bAMoveDirection:
cmp sreg
bcc @localIsSmaller

@localIsBiggerEq:
sec
sbc sreg

sty sreg
cmp sreg
bcc @dontMove

ldx #$2
bra @end

@dontMove:
ldx #$1

bra @end

@localIsSmaller:
ldx sreg
sta sreg
txa
sec
sbc sreg
sty sreg
cmp sreg
beq @successLocalIsSmaller
bcc @dontMove
@successLocalIsSmaller:
ldx #$0
@end:
rts

newdir:
    .byte 8, 1, 2
    .byte 7, 0, 3
    .byte 6, 5, 4

newdir_row_addresses:
    .addr newdir
    .addr newdir + 3         
    .addr newdir + 6         

_bAFollowEgoAsmSec:
;void bAFollowEgoAsmSec(ViewTable* localViewTab, ViewTable* egoViewTab, byte egoWidth, byte localCelWidth)
stp
.scope
MVT_LOCAL_VIEW_TAB = ZP_TMP_2
MVT_EGO_VIEW_TAB = ZP_TMP_3
MVT_EGO_WIDTH = ZP_TMP_4
MVT_LOCAL_CEL_WIDTH = ZP_TMP_4 + 1
MVT_ECX = ZP_TMP_5
MVT_LCX = ZP_TMP_5 + 1
MVT_DIFF_X = ZP_TMP_6
MVT_DIFF_Y = ZP_TMP_6 + 1
MVT_DELTA = ZP_TMP_7
MVT_DIR_VAL_X = ZP_TMP_8
MVT_DIR_VAL_Y = ZP_TMP_8 + 1
MVT_DIR = ZP_TMP_9

sta MVT_LOCAL_CEL_WIDTH
jsr popa
sta MVT_EGO_WIDTH
jsr popax
sta MVT_EGO_VIEW_TAB
stx MVT_EGO_VIEW_TAB + 1
jsr popax
sta MVT_LOCAL_VIEW_TAB
stx MVT_LOCAL_VIEW_TAB + 1

GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_EGO_VIEW_TAB, sreg
lda MVT_EGO_WIDTH
lsr
clc
adc sreg
sta MVT_ECX

GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_LOCAL_VIEW_TAB, sreg
lda MVT_LOCAL_CEL_WIDTH
lsr
clc
adc sreg
sta MVT_LCX

GET_STRUCT_8_STORED_OFFSET _offsetOfParam1, MVT_LOCAL_VIEW_TAB, MVT_DELTA

GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_EGO_VIEW_TAB,sreg
GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_LOCAL_VIEW_TAB

ldy MVT_DELTA
jsr bAMoveDirection
sta MVT_DIFF_X
stx MVT_DIR_VAL_X

GET_STRUCT_8_STORED_OFFSET _offsetOfYPos, MVT_EGO_VIEW_TAB,sreg
GET_STRUCT_8_STORED_OFFSET _offsetOfYPos, MVT_LOCAL_VIEW_TAB
ldy MVT_DELTA
jsr bAMoveDirection
sta MVT_DIFF_Y
stx MVT_DIR_VAL_Y

; php
; pha
; phx
; phy

; lda MVT_DIR_VAL_Y
; cmp #$1
; beq @continue
; stp

; @continue:
; ply
; plx
; pla
; plp


txa
asl
tax
lda newdir_row_addresses,x
sta sreg
lda newdir_row_addresses + 1,x
sta sreg + 1

ldy MVT_DIR_VAL_X
lda (sreg),y
sta MVT_DIR

stp
rts

.endscope

.endif