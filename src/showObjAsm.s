.include "x16.inc"

.ifndef SHOWOBJ_INC
SHOWOBJ_INC = 1

.segment "BANKRAM11"

SHOW_OBJ_ATTRIBUTE_ADDRESS = $1FFE0
MAX_SHOW_OBJ_SPLIT_SPRITES = $4

_b11ShowObjSpriteAddressShifted: .res 2 * MAX_SPRITES_ROW_OR_COLUMN_SIZE
_b11ShowObjSprAttr7: .byte $0
_b11ShowObjCelHeight: .byte $0
_b11ShowObjX: .word $0
_b11ShowObjY: .byte $0
_b11VeraSlots: .byte $0


b11ShowObjIrqHandler:
SET_VERA_ADDRESS_IMMEDIATE SHOW_OBJ_ATTRIBUTE_ADDRESS, #$0,#$1

lda _b11VeraSlots
asl
sta _b11VeraSlots

ldy #$0
@slotsLoop:
stp
;beq @increment

lda _b11ShowObjSpriteAddressShifted,y
sta VERA_data0
lda _b11ShowObjSpriteAddressShifted + 1,y 
sta VERA_data0
lda _b11ShowObjX
sta VERA_data0
lda _b11ShowObjX + 1
sta VERA_data0
lda _b11ShowObjY
sta VERA_data0
stz VERA_data0
lda #$C
sta VERA_data0
lda _b11ShowObjSprAttr7
sta VERA_data0

@increment:
iny
iny

cpy _b11VeraSlots
beq @end

clc
lda _b11ShowObjX
adc #TILE_LAYER_WIDTH
sta _b11ShowObjX
lda _b11ShowObjX + 1
adc #$0
sta _b11ShowObjX + 1

bra @slotsLoop

@end:
rts

b11ClearObjIrqHandler:
SET_VERA_ADDRESS_IMMEDIATE (SHOW_OBJ_ATTRIBUTE_ADDRESS + 6), #$0,#$4

lda #MAX_SHOW_OBJ_SPLIT_SPRITES

@loop:
stz VERA_data0
dec
bne @loop

rts

.endif