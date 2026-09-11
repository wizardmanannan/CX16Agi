.include "x16.inc"

.ifndef SHOWOBJ_INC
SHOWOBJ_INC = 1

.segment "BANKRAM11"

SHOW_OBJ_ATTRIBUTE_ADDRESS = $1FFF8

_b11ShowObjSpriteAddressShifted: .res 2
_b11ShowObjSprAttr7: .byte $0
_b11ShowObjCelHeight: .byte $0
_b11ShowObjX: .word $0
_b11ShowObjY: .byte $0

b11ShowObjIrqHandler:
stp


SET_VERA_ADDRESS_IMMEDIATE SHOW_OBJ_ATTRIBUTE_ADDRESS, #$0,#$1
stp
lda _b11ShowObjSpriteAddressShifted
sta VERA_data0
lda _b11ShowObjSpriteAddressShifted + 1
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

rts

.endif