.ifndef SPRITE_IRQ_HANDLER_INC
.include "globalGraphics.s"
.include "globalViews.s"
.include "helpersAsm.s"

.import _maxViewTable

.segment "BANKRAM0E"

.macro SET_VERA_START_SPRITE_ATTRS CHANNEL, STRIDE, OFFSET
SET_VERA_ADDRESS_IMMEDIATE (SPRITE_ATTR_START + SPRITE_ATTR_SIZE + OFFSET), CHANNEL, STRIDE ;Skips first which is for mouse
.endmacro

SPRITE_IRQ_HANDLER_INC = 1


BYTES_PER_SPRITE_UPDATE = 6
SPRITE_UPDATED_BUFFER_SIZE = 256 ;Viewtab may be updated more than once, hence times two for safety

.macro GET_NEXT_FROM_SPRITE_UPDATE_BUFFER noStore
.local @continue

lda _bESpritesUpdatedBuffer,y
iny
.ifblank noStore
  sta VERA_data0
.endif
@continue:
.endmacro


.macro CLEAR_SPRITE_ATTRS NO_TO_CLEAR
.local @outerLoop
.local @outerLoopCheck
.local @innerLoop
.local @innerLoopCheck
.local @end

ldy NO_TO_CLEAR
beq @end

SET_VERA_START_SPRITE_ATTRS #$0, #4, SA_VERA_ZORDER ;Set VERA channel 0 to first zorder attribute with a stride of 4.

@outerLoop:
stz VERA_data0 ; A zorder of zero means disabled

@outerLoopCheck:
dey
bne @outerLoop

@end:
.endmacro

_bESpritesUpdatedBuffer: .res SPRITE_UPDATED_BUFFER_SIZE
_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer

;void bEClearSpriteAttributes()
_bEClearSpriteAttributes:
lda #MAX_SPRITE_SLOTS
sta @numToClear

lda #IRQ_CMD_L0_L1_ONLY
ldx #$0
TRAMPOLINE #IRQ_BANK, _b6SetAndWaitForIrqStateAsm
CLEAR_SPRITE_ATTRS @numToClear

lda #IRQ_CMD_NORMAL
ldx #$0
TRAMPOLINE #IRQ_BANK, _b6SetAndWaitForIrqStateAsm
 
rts
@numToClear: .byte $0

;Define Insertion Order:
;0 Vera Address Sprite Data Middle (Low will always be 0) (If both the first two bytes are zero that indicates the end of the buffer)
;1 Vera Address Sprite Data High
;2 x low
;3 x high
;4 y
;5 Flipped
;6 Sprite Attr Size/Palette Offset

.import _viewSeen

ZP_SPR_ATTR_SIZE = ZP_TMP_5
ZP_LOW_BYTE = ZP_TMP_5 + 1
ZP_ADDRESS = ZP_TMP_6

bEHandleSpriteUpdates:

lda _bESpritesUpdatedBufferPointer ; If there is nothing in the buffer do nothing, leave the sprites as is
cmp #< _bESpritesUpdatedBuffer
bne @start
lda _bESpritesUpdatedBufferPointer + 1
cmp #> _bESpritesUpdatedBuffer
bne @start


jmp @end

@start:
CLEAR_SPRITE_ATTRS _maxViewTable

SET_VERA_START_SPRITE_ATTRS #$0, #$1, SA_VERA_ADDRESS_LOW ; Sets VERA channel 0 to the start of the sprites attributes table with a stride of 1

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

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ; 6 Collison ZDepth and Flip (buffer 5)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Sprite Attr Size 7 (buffer 6)

bra @loop
@addressReset:
lda #< _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #> _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1

@end:
rts
.endif