.ifndef SPRITE_IRQ_HANDLER_INC
.include "globalGraphics.s"
.include "globalViews.s"

.import _maxViewTable

.segment "BANKRAM0E"

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

SET_VERA_ADDRESS_IMMEDIATE (SPRITE_ATTR_START + SPRITE_ATTR_SIZE), #$0, #$1

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
;Define Insertion Order:
;0 Vera Address Sprite Data Middle (Low will always be 0) (If both the first two bytes are zero that indicates the end of the buffer)
;1 Vera Address Sprite Data High
;2 x
;3 y
;4 Sprite Attr Size
;5 Reblit on IRQ

_bESpritesUpdatedBuffer: .res SPRITE_UPDATED_BUFFER_SIZE
_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer

;void bEClearSpriteAttributes()
_bEClearSpriteAttributes:
lda #MAX_SPRITE_SLOTS
sta @numToClear

;CLEAR_SPRITE_ATTRS @numToClear

rts
@numToClear: .byte $0

bEHandleSpriteUpdates:

CLEAR_SPRITE_ATTRS _maxViewTable

lda #<_bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #>_bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1
rts

.endif