.include "x16.inc"

.ifndef SHOWOBJ_INC
SHOWOBJ_INC = 1

.segment "BANKRAM11"

; First reserved show.obj sprite-attribute slot (8 bytes each).
SHOW_OBJ_ATTRIBUTE_ADDRESS = $1FFE0
; How many of those slots CLEAR_OBJ will disable.
MAX_SHOW_OBJ_SPLIT_SPRITES = MAX_SPRITES_ROW_OR_COLUMN_SIZE
.assert MAX_SHOW_OBJ_SPLIT_SPRITES = 4, error, "show.obj must reserve four sprite slots"

; VRAM addresses >> 5, one 16-bit entry per Vera slot. Filled by C;
; written here as sprite attrs 0–1.
_b11ShowObjSpriteAddressShifted: .res 2 * MAX_SPRITES_ROW_OR_COLUMN_SIZE
; Sprite attr 7: H-size << 6 | W-size << 4 | palette. Packed in C.
_b11ShowObjSprAttr7: .byte $0
; Pixel position of slot 0. Extra slots step X by SPR_SIZE_64.
_b11ShowObjX: .word $0
_b11ShowObjY: .byte $0
; Number of VERA slots C allocated.
_b11VeraSlots: .byte $0

; Private cursors keep each IRQ publication repeatable and the C inputs intact.
b11ShowObjSlotEnd: .byte $0
b11ShowObjSlotX: .word $0


; Program the show.obj sprite attributes from the C globals.
; Entered via b6SetAndWaitForIrqState(SHOW_OBJ).
b11ShowObjIrqHandler:
lda _b11VeraSlots
beq @end
cmp #(MAX_SHOW_OBJ_SPLIT_SPRITES + 1)
bcs @end
asl
sta b11ShowObjSlotEnd
lda _b11ShowObjX
sta b11ShowObjSlotX
lda _b11ShowObjX + 1
sta b11ShowObjSlotX + 1

; VERA port 0 at $1FFE0, increment 1 — walk attrs byte by byte.
SET_VERA_ADDRESS_IMMEDIATE SHOW_OBJ_ATTRIBUTE_ADDRESS, #$0,#$1

ldy #$0
@slotsLoop:
; Attrs 0–1: sprite data address (already >> 5).
lda _b11ShowObjSpriteAddressShifted,y
sta VERA_data0
lda _b11ShowObjSpriteAddressShifted + 1,y
sta VERA_data0
; Attrs 2–3: 16-bit X of this slot.
lda b11ShowObjSlotX
sta VERA_data0
lda b11ShowObjSlotX + 1
sta VERA_data0
; Attrs 4–5: Y (8-bit) + Y high = 0.
lda _b11ShowObjY
sta VERA_data0
stz VERA_data0
; Attr 6: $0C = enabled, 4bpp, z-depth on top.
lda #$C
sta VERA_data0
; Attr 7: size bits + palette from C.
lda _b11ShowObjSprAttr7
sta VERA_data0

@increment:
iny
iny

cpy b11ShowObjSlotEnd
beq @end

; Next split slot sits SPR_SIZE_64 pixels to the right.
clc
lda b11ShowObjSlotX
adc #SPR_SIZE_64
sta b11ShowObjSlotX
lda b11ShowObjSlotX + 1
adc #$0
sta b11ShowObjSlotX + 1

bra @slotsLoop

@end:
rts

; Disable the reserved show.obj sprites.
; Entered via b6SetAndWaitForIrqState(CLEAR_OBJ).
b11ClearObjIrqHandler:
; Start at attr 6 of the first slot (enable / z / mode).
; Increment 4 lands on attr 6 of each following 8-byte slot.
SET_VERA_ADDRESS_IMMEDIATE (SHOW_OBJ_ATTRIBUTE_ADDRESS + 6), #$0,#$4

lda #MAX_SHOW_OBJ_SPLIT_SPRITES

@loop:
stz VERA_data0          ; clear enable — sprite off
dec
bne @loop

rts

.endif
