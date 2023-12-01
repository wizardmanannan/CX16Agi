.ifndef SPRITE_IRQ_HANDLER_INC
.include "globalGraphics.s"
.include "globalViews.s"

.import _maxViewTable

.segment "BANKRAM0E"

.macro SET_VERA_START_SPRITE_ATTRS
SET_VERA_ADDRESS_IMMEDIATE (SPRITE_ATTR_START + SPRITE_ATTR_SIZE), #$0, #$1 ;Skips first which is for mouse
.endmacro

SPRITE_IRQ_HANDLER_INC = 1


BYTES_PER_SPRITE_UPDATE = 7
SPRITE_UPDATED_BUFFER_SIZE = VIEW_TABLE_SIZE * BYTES_PER_SPRITE_UPDATE * 2 ;Viewtab may be updated more than once, hence times two for safety

.macro CLEAR_SPRITE_ATTRS NO_TO_CLEAR
.local @outerLoop
.local @outerLoopCheck
.local @innerLoop
.local @innerLoopCheck
.local @end

ldx NO_TO_CLEAR
beq @end

SET_VERA_START_SPRITE_ATTRS

@outerLoop:
ldy #SPRITE_ATTR_SIZE
@innerLoop:
stz VERA_data0


@innerLoopCheck:
dey
bne @innerLoop

@outerLoopCheck:
dex 
bne @outerLoop

@end:
.endmacro

_bESpritesUpdatedBuffer: .res 256
_bESpritesUpdatedBufferHigh: .byte SPRITE_UPDATED_BUFFER_SIZE - 256
_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer

;void bEClearSpriteAttributes()
_bEClearSpriteAttributes:
lda #MAX_SPRITE_SLOTS
sta @numToClear

;CLEAR_SPRITE_ATTRS @numToClear

rts
@numToClear: .byte $0

;Define Insertion Order:
;0 Vera Address Sprite Data Middle (Low will always be 0) (If both the first two bytes are zero that indicates the end of the buffer)
;1 Vera Address Sprite Data High
;2 x
;3 y
;4 Sprite Attr Size
;5 Reblit on IRQ



ZP_SPR_ATTR_SIZE = ZP_TMP_5
ZP_LOW_BYTE = ZP_TMP_5 + 1
ZP_ADDRESS = ZP_TMP_6
bEHandleSpriteUpdates:
lda _bESpritesUpdatedBufferPointer
cmp #< _bESpritesUpdatedBuffer
bne @start
lda _bESpritesUpdatedBufferPointer + 1
cmp #> _bESpritesUpdatedBuffer
bne @start


jmp @end

@start:
CLEAR_SPRITE_ATTRS _maxViewTable

SET_VERA_START_SPRITE_ATTRS

ldy #$0

lda #< _bESpritesUpdatedBuffer
sta ZP_ADDRESS
lda #> _bESpritesUpdatedBuffer
sta ZP_ADDRESS + 1

@loop:
lda (ZP_ADDRESS),y ;Address 12:5 0 (buffer 0)
iny
beq @loopHigh
sta VERA_data0
sta ZP_LOW_BYTE

lda (ZP_ADDRESS),y ;Address 16:13 1 (buffer 1)
iny
beq @loopHigh
sta VERA_data0

ora ZP_LOW_BYTE
beq @addressReset

lda (ZP_ADDRESS),y ;X Low 2 (buffer 2)
iny
beq @loopHigh
sta VERA_data0

stz VERA_data0 ;X High Always 0 3

lda (ZP_ADDRESS),y ;Y Low 4 (buffer 3)
iny
beq @loopHigh
sta VERA_data0

stz VERA_data0 ;Y High Always 5

lda #$C ; Collision ZLvl and Flip 6 (C means in front of layers and not flipped, with a zero collision mask)
sta VERA_data0

lda (ZP_ADDRESS),y ;Sprite Attr Size 7 (buffer 4)
iny
beq @loopHigh

asl
asl
asl
asl
sta ZP_SPR_ATTR_SIZE
asl
asl
ora ZP_SPR_ATTR_SIZE
sta VERA_data0

lda (ZP_ADDRESS),y ;Reblit (buffer 5) Reblit ignore for now
iny
beq @loopHigh
sta VERA_data0

bra @loop
@loopHigh:
@addressReset:
lda #< _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #> _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1

@end:
rts
.endif