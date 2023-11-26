.ifndef SPRITE_IRQ_HANDLER_INC
.include "globalGraphics.s"
.include "globalViews.s"

.segment "BANKRAM0E"

SPRITE_IRQ_HANDLER_INC = 1


BYTES_PER_SPRITE_UPDATE = 7
SPRITE_UPDATED_BUFFER_SIZE = VIEW_TABLE_SIZE * BYTES_PER_SPRITE_UPDATE * 2 ;Viewtab may be updated more than once, hence times two for safety

;Define Insertion Order:
;0 Vera Address Sprite Data Middle (Low will always be 0) (If both the first two bytes are zero that indicates the end of the buffer)
;1 Vera Address Sprite Data High
;2 Sprite Attribute Address Low (High will always be 1)
;3 Sprite Attribute Address Middle 
;4 x
;5 y
;6 Reblit on IRQ

_bESpritesUpdatedBuffer: .res SPRITE_UPDATED_BUFFER_SIZE
_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer

bEHandleSpriteUpdates:
lda #<_bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #>_bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1
rts

.endif