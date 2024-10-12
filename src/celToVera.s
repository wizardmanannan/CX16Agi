.ifndef CELTOVERA_INC

CELTOVERA_INC = 1

.include "viewCommon.s"
.include "lineDrawing.s"
.segment "CODE"

.macro READ_NEXT_BYTE
.local @increment
.local @end
ldy NEXT_DATA_INDEX
lda (BMP_DATA), y

inc NEXT_DATA_INDEX

bne @end

@increment:
inc BMP_DATA + 1 ;Adding 256 0x100 which is adding zero to the low byte and 1 to the high
@end:
.endmacro

newIncrementValue: .byte $00, %10000, $00, $00, $00, $00, $00, $00  ; 8 zeros
    .byte $00, $00, $00, $00, $00, $00, $00, $00 ; 8 more zeros
    .byte %10000, $00

celToVeraLowRam:
stz NEXT_DATA_INDEX

lda RAM_BANK
pha

lda CEL_BANK
sta RAM_BANK

GET_STRUCT_8_STORED_OFFSET _offsetOfCelHeight, CEL_ADDR, CEL_HEIGHT
GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, CEL_ADDR, CEL_TRANS

lda SPLIT_SEGMENTS
cmp #$1
bne celToVeraLowRam_split

celToVeraLowRam_nonSplit:
GET_STRUCT_16_STORED_OFFSET _offsetOfBmp, CEL_ADDR, BMP_DATA ;Buffer status holds the C struct to be passed to b5RefreshBuffer
GET_STRUCT_8_STORED_OFFSET _offsetOfBmpBank, CEL_ADDR, BMP_BANK
bra celToVeraLowRam_start


celToVeraLowRam_split:
lda SPLIT_COUNTER 
dec
asl

tay

lda SPLIT_CEL_BANK
sta BMP_BANK
sta RAM_BANK

lda (SPLIT_CEL_SEGMENTS), y
sta BMP_DATA 
iny
lda (SPLIT_CEL_SEGMENTS), y
sta BMP_DATA + 1

celToVeraLowRam_start:
.ifdef DEBUG_VIEW_DRAW
stz _logDebugVal1
.endif

lda CEL_TRANS
SET_COLOR_LEFT CEL_TRANS
sta CEL_TRANS

lda BMP_BANK
sta RAM_BANK

celToVeraLowRam_setVeraAddress:
SET_VERA_ADDRESS VERA_ADDRESS, #$1, VERA_ADDRESS_HIGH, #$0
ldy Y_VAL
CALC_VRAM_ADDR_PRIORITY X_VAL, #$1
celToVeraLowRam_calculatePriorityAutoInc: ;This calculates whether the priority auto incrementment should be initially switched on or not. If X even auto inc should be off, because we need to read the same byte for the next turn
lda X_VAL
lsr
bcs celToVeraLowRam_oddValue

celToVeraLowRam_evenValue:
stz VERA_addr_bank ;Disable, we need to read this byte again
bra celToVeraLowRam_getNextChunk

celToVeraLowRam_oddValue:
lda #%10000 ;Odd on the next read we should move onto the next byte
sta VERA_addr_bank

celToVeraLowRam_getNextChunk:
READ_NEXT_BYTE
cmp #$0 ;If its zero we are finished with this line
bne celToVeraLowRam_processChunk
jmp celToVeraLowRam_increment
celToVeraLowRam_processChunk:
tax
and #$0F; The number of pixels is the lower 4 bits
tay
txa
and #$F0; The colour is the upper 4 bits
sta COLOR
SET_COLOR_RIGHT COLOR ;Output color must be 'doubled up', because AGI pixels are doubled across
sta COLOR

cmp CEL_TRANS
bne celToVeraLowRam_draw

celToVeraLowRam_skip: ;When 'drawing' transparent pixels we still need to increment the address
celToVeraLowRam_spriteMemoryAdd:
stz VERA_ctrl
tya
clc
adc VERA_addr_low
sta VERA_addr_low

bcc celToVeraLowRam_prepareForPriorityAdd

lda #$0
adc VERA_addr_high
sta VERA_addr_high

celToVeraLowRam_prepareForPriorityAdd:
lda #$1
sta VERA_ctrl

tya
and #1
ora VERA_addr_bank
tax
cmp #17
bne celToVeraLowRam_noExtraAddRequired
celToVeraLowRam_extraAddRequired:
tya
inc
lsr
bra celToVeraLowRam_addPriority
celToVeraLowRam_noExtraAddRequired:
tya
lsr

celToVeraLowRam_addPriority:
clc
adc VERA_addr_low
sta VERA_addr_low
bcc celToVeraLowRam_determineIncrementValue
lda #$0
adc VERA_addr_high
sta VERA_addr_high

celToVeraLowRam_determineIncrementValue:
lda newIncrementValue,x
sta VERA_addr_bank

jmp celToVeraLowRam_getNextChunk

celToVeraLowRam_draw:

celToVeraLowRam_drawBlack:
cmp #BLACK_COLOR
bne celToVeraLowRam_checkPriority
lda CEL_TRANS ;Black is swapped with the transparent colour in the case the transparent colour is not black
sta COLOR

celToVeraLowRam_checkPriority:
ldx VERA_data1 ;Get the next priority byte and toggle
lda #%10000
eor VERA_addr_bank
sta VERA_addr_bank

bne celToVeraLowRam_getEvenValue ;If we toggled this to be on then that means we will read the byte one more time, and then increment. This must mean that this time we are reading the even nibble

celToVeraLowRam_getOddValue:
txa ;Read the right hand side
and #$0F
bra celToVeraLowRam_comparePriority

celToVeraLowRam_getEvenValue:
txa ;Read the left hand side
lsr 
lsr
lsr
lsr

celToVeraLowRam_comparePriority:
cmp #NOT_AN_OBSTACLE
beq celToVeraLowRam_drawColor ;4 is the lowest priority and the object always has precedence 
bcc celToVeraLowRam_drawColor ;TODO: Is a control value need to use the rules in split priority to figure out what the priority is
cmp P_NUM
beq celToVeraLowRam_drawColor ;If the object and screen priority is equal the object has precedence
bcc celToVeraLowRam_drawColor ;If the object screen priority < object priority the object has precedence

celToVeraLowRam_skipBasedOnPriority: ;This means that the screen priority > object priority, so we don't draw
stz VERA_data0 ;Warning: _b9CelToVera self modifies this line to ldx
lda COLOR
bra celToVeraLowRam_decrementColorCounter

celToVeraLowRam_drawColor: ;This means that the screen priority <= object priority, so we do draw
lda COLOR
sta VERA_data0

celToVeraLowRam_decrementColorCounter:
dey
bne celToVeraLowRam_checkPriority ;If y is not zero we are not yet finished with this run encoded byte. Note: If the color is black, then its swapped position is at this point already stored in COLOR, so we don't need to do a draw black check again
jmp celToVeraLowRam_getNextChunk

celToVeraLowRam_increment:
dec CEL_HEIGHT ;Ready for the next line
beq celToVeraLowRam_end

clc
lda BYTES_PER_ROW 
adc VERA_ADDRESS
sta VERA_ADDRESS
bcc celToVeraLowRam_incrementY

lda #$0
adc VERA_ADDRESS + 1
sta VERA_ADDRESS + 1
bcc celToVeraLowRam_incrementY

lda #$0
adc VERA_ADDRESS_HIGH
sta VERA_ADDRESS_HIGH

celToVeraLowRam_incrementY:
inc Y_VAL
lda Y_VAL ;If Y_VAL is greater than the height then the object is partially off screen, stop drawing altogether
cmp #PICTURE_HEIGHT
bcs celToVeraLowRam_end

jmp celToVeraLowRam_setVeraAddress

celToVeraLowRam_end:
pla 
sta RAM_BANK
rts
.endif
