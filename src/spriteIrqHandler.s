.ifndef SPRITE_IRQ_HANDLER_INC
.include "globalGraphics.s"
.include "globalViews.s"
.include "helpersAsm.s"
.include "celToVeraConstants.s"
.include "celToVera.s"
.include "lineDrawing.s"

.segment "BANKRAM0E"

.macro SET_VERA_START_SPRITE_ATTRS CHANNEL, STRIDE, OFFSET
SET_VERA_ADDRESS_IMMEDIATE (SPRITE_ATTR_START + SPRITE_ATTR_SIZE + OFFSET), CHANNEL, STRIDE ;Skips first which is for mouse
.endmacro

SPRITE_IRQ_HANDLER_INC = 1


BYTES_PER_SPRITE_UPDATE = 8
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


.macro CLEAR_SPRITE_ATTRS
.local @loop
.local @end


SET_VERA_START_SPRITE_ATTRS #$1, #4, SA_VERA_ZORDER ;Set VERA channel 0 to first zorder attribute with a stride of 4.
lda VERA_addr_low
ldx VERA_addr_high
ldy VERA_addr_bank
stz VERA_ctrl
sta VERA_addr_low
stx VERA_addr_high
sty VERA_addr_bank

@loop:
lda VERA_data0 ; A zorder of zero means disabled
beq @end
stz VERA_data1
bra @loop
@end:
.endmacro

.macro CLEAR_ACTIVE_SPRITE_ATTRS ;Excludes mouse
.local @loop
.local @end

SET_VERA_START_SPRITE_ATTRS #$0, #4, SA_VERA_ZORDER ;Set VERA channel 0 to first zorder attribute with a stride of 4.
SET_VERA_START_SPRITE_ATTRS #$1, #4, SA_VERA_ZORDER ;Set VERA channel 1 so we can see if we have reached the end
ldy #$0

@loop:
lda VERA_data1
and #$C ;Mask out non zorder bits
beq @end

stz VERA_data0 ; A zorder of zero means disabled
iny
cpy #NO_SPRITES
beq @end ;Make sure we don't overrun

bra @loop

@end:
.endmacro

_bESpritesUpdatedBuffer: .res SPRITE_UPDATED_BUFFER_SIZE
_bESpritesUpdatedBufferPointer: .word _bESpritesUpdatedBuffer



bECalculateTotalRows:
cmp #SPR_ATTR_8

bne @check16H

@setWidth8H:
lda #SPRITE_TOTAL_ROWS_8
sta TOTAL_ROWS

bra @end

@check16H:
cmp #SPR_ATTR_16
bne @check32H

@setWidth16H:
lda #SPRITE_TOTAL_ROWS_16
sta TOTAL_ROWS
bra @end

@check32H:
cmp #SPR_ATTR_32
bne @setWidth64H

@setWidth32H:
lda #SPRITE_TOTAL_ROWS_32
sta TOTAL_ROWS
bra @end

@setWidth64H:
lda #SPRITE_TOTAL_ROWS_64
sta TOTAL_ROWS
bra @end

@end:
rts

bESetBytesPerRow:
cmp #SPR_ATTR_8

bne @check16W

@setWidth8W:
lda #BYTES_PER_ROW_8
sta BYTES_PER_ROW

bra @end

@check16W:
cmp #SPR_ATTR_16
bne @check32W

@setWidth16W:
lda #BYTES_PER_ROW_16
sta BYTES_PER_ROW
bra @end

@check32W:
cmp #SPR_ATTR_32
bne @setWidth64W

@setWidth32W:
lda #BYTES_PER_ROW_32
sta BYTES_PER_ROW
bra @end

@setWidth64W:
lda #BYTES_PER_ROW_64
sta BYTES_PER_ROW

@end:
rts


;void bEClearSpriteAttributes()
_bEClearSpriteAttributes:
CLEAR_SPRITE_ATTRS
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
;7 Reblit

.import _viewSeen

ZP_SPR_ATTR_SIZE = ZP_TMP_5
ZP_LOW_BYTE = ZP_TMP_5 + 1

bEHandleSpriteUpdates:

lda _bESpritesUpdatedBufferPointer ; If there is nothing in the buffer do nothing, leave the sprites as is
cmp #< _bESpritesUpdatedBuffer
bne @start
lda _bESpritesUpdatedBufferPointer + 1
cmp #> _bESpritesUpdatedBuffer
bne @start


jmp @end

@start:

CLEAR_SPRITE_ATTRS
.import _sResetCounter

SET_VERA_START_SPRITE_ATTRS #$0, #$1, SA_VERA_ADDRESS_LOW ; Sets VERA channel 0 to the start of the sprites attributes table with a stride of 1

ldy #$0
@loop:
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Address 12:5 0 (buffer 0)
sta ZP_LOW_BYTE

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Address 16:13 1 (buffer 1)

ora ZP_LOW_BYTE
bne @displaySprites
jmp @addressReset

@displaySprites:
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;X Low 2 (buffer 2)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;X High 3 (buffer 3)

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Y Low 4 (buffer 4)

stz VERA_data0 ;Y High 5 Always 0

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ; 6 Collison ZDepth and Flip (buffer 5)
and #1
sta @flipped

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER ;Sprite Attr Size 7 (buffer 6)

;Reblit
stz VERA_ADDRESS ;Always zero
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; LoopVeraAddress
sta VERA_ADDRESS + 1
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 
sta VERA_ADDRESS_HIGH

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; Address of the Cel 
sta CEL_ADDR
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1
sta CEL_ADDR + 1

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; Cel Bank
sta CEL_BANK 

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; X VAL
sta X_VAL
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; Y VAL
sta Y_VAL
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ; Priority
sta P_NUM

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ;Allocation Width
jsr bESetBytesPerRow

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ;Allocation Height
jsr bECalculateTotalRows

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ;Split Cel Pointers
sta SPLIT_CEL_SEGMENTS
GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 ;Split Cel Pointers + 1
sta SPLIT_CEL_SEGMENTS + 1

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 
sta SPLIT_CEL_BANK

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 
sta SPLIT_COUNTER

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 
sta SPLIT_SEGMENTS

GET_NEXT_FROM_SPRITE_UPDATE_BUFFER #$1 
cmp #$1
beq @reblitMotion
jmp @loop

@reblitMotion:
lda VERA_addr_low
pha
lda VERA_addr_high
pha
lda VERA_addr_bank
pha
phy

lda @flipped
bne @celToVeraBackwards
;jsr celToVera
bra @returnFromCelToVera
@celToVeraBackwards:
jsr bECelToVeraBackwards

@returnFromCelToVera:
ply
stz VERA_ctrl
pla
sta VERA_addr_bank
pla 
sta VERA_addr_high
pla
sta VERA_addr_low

jmp @loop
@addressReset:
lda #< _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer
lda #> _bESpritesUpdatedBuffer
sta _bESpritesUpdatedBufferPointer + 1

@end:

rts
@flipped: .byte $0
.endif