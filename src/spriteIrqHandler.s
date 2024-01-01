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

.macro GET_NEXT_FROM_SPRITE_UPDATE_BUFFER noStore
.local @continue

lda (ZP_ADDRESS),y
iny
.ifblank noStore
  sta VERA_data0
.endif
cpy #$0
bne @continue
jsr _bEGotoHighAddresses

@continue:
.endmacro


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
;2 x low
;3 x high
;4 y
;5 Sprite Attr Size/Palette Offset
;6 Reblit on IRQ

.import _viewSeen

ZP_SPR_ATTR_SIZE = ZP_TMP_5
ZP_LOW_BYTE = ZP_TMP_5 + 1
ZP_ADDRESS = ZP_TMP_6

_bEGotoHighAddresses:
clc
lda ZP_ADDRESS
adc #$FF
sta ZP_ADDRESS
lda ZP_ADDRESS + 1
adc #0
sta ZP_ADDRESS + 1

ldy #$0

rts

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

lda #< _bESpritesUpdatedBuffer
sta ZP_ADDRESS
lda #> _bESpritesUpdatedBuffer
sta ZP_ADDRESS + 1

@loop:
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Address 12:5 0 (buffer 0)
sta ZP_LOW_BYTE

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Address 16:13 1 (buffer 1)

ora ZP_LOW_BYTE
beq @addressReset

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;X Low 2 (buffer 2)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;X High 3 (buffer 3)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Y Low 4 (buffer 4)

stz VERA_data0 ;Y High 5 Always 0

lda #$8 ; Collision Z Lvl 2 and Flip 6 (8 means in front of bitmap but behind text layers and not flipped, with a zero collision mask)
sta VERA_data0

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Sprite Attr Size 7 (buffer 5)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ;Reblit (buffer 6) Reblit ignore for now

bra @loop
@addressReset:
lda #< _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #> _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1

@end:
rts
.endif