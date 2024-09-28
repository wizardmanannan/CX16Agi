.ifndef CELTOVERA_INC

CELTOVERA_INC = 1

.include "viewCommon.s"
.include "lineDrawing.s"
.segment "CODE"


.macro READ_NEXT_BYTE
.local @increment
.local @end

ldy NEXT_DATA_INDEX
lda (CEL_DATA), y

inc NEXT_DATA_INDEX

bne @end

@increment:
inc CEL_DATA + 1 ;Adding 256 0x100 which is adding zero to the low byte and 1 to the high
@end:
.endmacro

celToVeraLowRam:
.scope
lda X_VAL 
sta X_VAL_ORIG ;Keep track of the original X value

stz NEXT_DATA_INDEX

lda RAM_BANK
pha

lda CEL_BANK
sta RAM_BANK

GET_STRUCT_8_STORED_OFFSET _offsetOfCelHeight, CEL_ADDR, CEL_HEIGHT
GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, CEL_ADDR, CEL_TRANS
stz BUFFER_STATUS + 3

lda SPLIT_SEGMENTS
cmp #$1
bne @split

@nonSplit:
GET_STRUCT_16_STORED_OFFSET _offsetOfBmp, CEL_ADDR, CEL_DATA ;Buffer status holds the C struct to be passed to b5RefreshBuffer
GET_STRUCT_8_STORED_OFFSET _offsetOfBmpBank, CEL_ADDR, CEL_DATA_BANK
bra @start

@split:
lda SPLIT_COUNTER
asl
tay

lda bESplitAddressesBuffer, y
sta BUFFER_STATUS 
iny
lda bESplitAddressesBuffer, y
sta BUFFER_STATUS + 1

lda SPLIT_CEL_BANK
sta BUFFER_STATUS + 2

@start:

.ifdef DEBUG_VIEW_DRAW
stz _logDebugVal1
.endif

lda CEL_TRANS
SET_COLOR_LEFT CEL_TRANS
sta CEL_TRANS

lda CEL_DATA_BANK
sta RAM_BANK

@resetXCounter:
lda X_VAL_ORIG ;Restore the original X value
sta X_VAL

@setVeraAddress:
SET_VERA_ADDRESS VERA_ADDRESS, #$1, VERA_ADDRESS_HIGH, #$0
ldy Y_VAL
CALC_VRAM_ADDR_PRIORITY X_VAL, #$1
@calculatePriorityAutoInc: ;This calculates whether the priority auto incrementment should be initially switched on or not. If X even auto inc should be off, because we need to read the same byte for the next turn
lda X_VAL
lsr
bcs @oddValue

@evenValue:
stz VERA_addr_bank ;Disable, we need to read this byte again
bra @getNextChunk

@oddValue:
lda #%10000 ;Odd on the next read we should move onto the next byte
sta VERA_addr_bank

@getNextChunk:
READ_NEXT_BYTE
cmp #$0 ;If its zero we are finished with this line
beq @increment
tax
and #$0F; The number of pixels is the lower 4 bits
tay
txa
and #$F0; The colour is the upper 4 bits
sta OUTPUT_COLOUR
SET_COLOR_RIGHT OUTPUT_COLOUR ;Output color must be 'doubled up', because AGI pixels are doubled across

cmp CEL_TRANS
bne @draw

@skip: ;When 'drawing' transparent pixels we still need to increment the address
ldx VERA_data0 ;We are not changing this one so we load load in order to increment and ignore the value
ldx VERA_data1
pha ;Toggle the priority auto increment and preserve the color
lda #%10000
eor VERA_addr_bank
sta VERA_addr_bank
pla

inc X_VAL ;Increment the x val. Note this is not used in calculations only for bounds checks
dey ;Have we handled every pixel in this run encoded byte
beq @getNextChunk
bra @skip

@draw:
ldx X_VAL
cpx #PICTURE_WIDTH ;If X is larger than the width, the object is at least partially off screen we skip drawing the rest of the pixels on this line
bcs @skip 

@drawBlack:
cmp #BLACK_COLOR
bne @checkPriority
lda CEL_TRANS ;Black is swapped with the transparent colour in the case the transparent colour is not black

@checkPriority:
ldx VERA_data1 ;Get the next priority byte and toggle
pha ;Preserve the color
lda #%10000
eor VERA_addr_bank
sta VERA_addr_bank

bne @getEvenValue ;If we toggled this to be on then that means we will read the byte one more time, and then increment. This must mean that this time we are reading the even nibble

@getOddValue:
txa ;Read the right hand side
and #$0F
bra @comparePriority

@getEvenValue:
txa ;Read the left hand side
lsr 
lsr
lsr
lsr

@comparePriority:
cmp #NOT_AN_OBSTACLE
beq @drawColor ;4 is the lowest priority and the object always has precedence 
bcc @drawColor ;TODO: Is a control value need to use the rules in split priority to figure out what the priority is
cmp P_NUM
beq @drawColor ;If the object and screen priority is equal the object has precedence
bcc @drawColor ;If the object screen priority < object priority the object has precedence

@skipBasedOnPriority: ;This means that the screen priority > object priority, so we don't draw
lda VERA_data0
pla ;Retrieve the color
bra @decrementColorCounter

@drawColor: ;This means that the screen priority <= object priority, so we do draw
pla ;Retrieve the color
sta VERA_data0

@decrementColorCounter:
dey
bne @draw ;If y is not zero we are not yet finished with this run encoded byte
jmp @getNextChunk

@increment:
dec CEL_HEIGHT ;Ready for the next line
beq @end

clc
lda BYTES_PER_ROW 
adc VERA_ADDRESS
sta VERA_ADDRESS
lda #$0
adc VERA_ADDRESS + 1
sta VERA_ADDRESS + 1
lda #$0
adc VERA_ADDRESS_HIGH
sta VERA_ADDRESS_HIGH

inc Y_VAL

lda Y_VAL ;If Y_VAL is greater than the height then the object is partially off screen, stop drawing altogether
cmp #PICTURE_HEIGHT
bcs @end

jmp @resetXCounter

@end:
pla 
sta RAM_BANK
rts
.endscope
.endif
