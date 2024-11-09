.ifndef CELTOVERA_INC

CELTOVERA_INC = 1

.include "celToVeraConstants.s"
.include "lineDrawing.s"

.segment "BANKRAM09"
_viewHeaderBuffer: .res VIEW_HEADER_BUFFER_SIZE
_loopHeaderBuffer: .res LOOP_HEADER_BUFFER_SIZE

;Constants
NO_MARGIN = 4
;void b9CelToVera(Cel* localCel, byte celBank, long veraAddress, byte drawingAreaWidth, byte x, byte y, byte pNum)
_b9CelToVera:
sta P_NUM
jsr popax
sta Y_VAL
stx X_VAL
jsr popa
sta BYTES_PER_ROW
jsr popax
sta VERA_ADDRESS
stx VERA_ADDRESS + 1
jsr popax
sta VERA_ADDRESS_HIGH
stx VERA_ADDRESS_HIGH + 1
jsr popa
sta CEL_BANK
jsr popax
sta CEL_ADDR
stx CEL_ADDR + 1
lda #$1
sta SPLIT_SEGMENTS ;When we draw directly to the bitmap we don't need to split the cel into segments

sei
lda celToVeraLowRam_skipBasedOnPriority
pha
lda #LDX_ABS
sta celToVeraLowRam_skipBasedOnPriority
lda #$1
sta CEL_TO_VERA_IS_FORWARD_DIRECTION
jsr celToVera
pla
sta celToVeraLowRam_skipBasedOnPriority
REENABLE_INTERRUPTS
rts

.segment "BANKRAM0E"

;When calling celToVera on a flipped cel the celToVera priority byte order must be backwards.
;This means that for a sprite which is 6 wide, when drawing pixel zero priority pixel 5 must be check, and when drawing 1 4 must be checked and so on. 
;This function is a helper function which modifies the code of celToVera to run backwards.
;All of the spots in celToVera requiring modification, are labelled and commented with self modify and the value it is modified to. 
;This function backs the old instructions up using the stack.
;We also need to modify X value to be at the right side of the sprite, not the left.
bECelToVeraBackwards:
clc ;Add the cel width and take 1 to set X to the right hand side of the sprite.
lda X_VAL 
adc CEL_WIDTH
dec
sta X_VAL

;Store the old instructions on the stack and self modify the celToVera function to make it run backwards
lda celToVeraLowRam_addPriority
pha
lda #SEC_IMP
sta celToVeraLowRam_addPriority

lda celToVeraLowRam_addPriorityLow
pha
lda #SBC_ZP
sta celToVeraLowRam_addPriorityLow

lda celToVeraLowRam_branchAfterLowByte
pha
lda #BCS_IMP
sta celToVeraLowRam_branchAfterLowByte

lda celToVeraLowRam_addPriorityHigh
pha
lda #SBC_IMM
sta celToVeraLowRam_addPriorityHigh

lda celToVeraLowRam_setPrioritySetDirection + 1
pha
lda #%1000
sta celToVeraLowRam_setPrioritySetDirection + 1

lda celToVeraLowRam_evenValue + 1
pha 
lda #%11000
sta celToVeraLowRam_evenValue + 1

lda celToVeraLowRam_oddValue + 1
pha
lda #%1000 ;self modify 2 to lda #%1000
sta celToVeraLowRam_oddValue + 1

lda celToVeraLowRam_prepareForPriorityCmp + 1
pha
lda #%11001
sta celToVeraLowRam_prepareForPriorityCmp + 1

lda celToVeraLowRam_determineIncrementValue + 1
pha
lda #<newIncrementBackwards
sta celToVeraLowRam_determineIncrementValue + 1

lda celToVeraLowRam_determineIncrementValue + 2
pha
lda #>newIncrementBackwards
sta celToVeraLowRam_determineIncrementValue + 2

lda celToVeraLowRam_checkPriorityCmp + 1
pha
lda #%11000
sta celToVeraLowRam_checkPriorityCmp + 1

stz CEL_TO_VERA_IS_FORWARD_DIRECTION ;Disable the forward flag

jsr celToVera

;Restore the old code back
pla
sta celToVeraLowRam_checkPriorityCmp + 1

pla 
sta celToVeraLowRam_determineIncrementValue + 2
pla
sta celToVeraLowRam_determineIncrementValue + 1 

pla
sta celToVeraLowRam_prepareForPriorityCmp + 1

pla 
sta celToVeraLowRam_oddValue + 1

pla 
sta celToVeraLowRam_evenValue + 1

pla 
sta celToVeraLowRam_setPrioritySetDirection + 1

pla 
sta celToVeraLowRam_addPriorityHigh

pla
sta celToVeraLowRam_branchAfterLowByte


pla 
sta celToVeraLowRam_addPriorityLow

pla
sta celToVeraLowRam_addPriority

rts

bEFindPriorityFromCtrlLineGoBackIncrementer: .byte %10000, %11000 

;When a control line is encountered when checking for the priority byte, we obtain the priority by searching down the screen (+) until we find a priority pixel. 
;If the incrementor is off then that means we have just advanced, and we will need to so we will have to either subtract (forwards direction) or add one to the address (backwards direction).
;We preserve VERA initially and restore it afterwards

bEFindPriorityFromCtrlLine:

lda #$1 
sta VERA_ctrl

lda VERA_addr_low ;Preserve the VERA
pha
lda VERA_addr_high
pha
lda VERA_addr_bank
pha

lda VERA_addr_bank ;If the incrementor is off we need to go back one. The incrementor is bit 4, so if it is on and you EOR with 1 you get zero. Hence we don't go back one on zero
eor #%10000
bne @backOne

@dontGoBackOne:
ldy CEL_TO_VERA_IS_FORWARD_DIRECTION ;Store in Y whether we are in forward direction mode or not. Later on if we are going forward, then that means we will read the even nibble otherwise the odd nibble
bra @findCtrlValue

@backOne:
lda CEL_TO_VERA_IS_FORWARD_DIRECTION
tax
eor #$1 ;Store in y the inverse of forward direction. Later on if we are going forward, then that means we will read the odd nibble otherwise the even nibble
tay

lda bEFindPriorityFromCtrlLineGoBackIncrementer,x ;Go back one using auto increment. If we are in forwards mode backwards means -1, otherwise the opposite.
sta VERA_addr_bank
lda VERA_data1

;This loop will check to see whether the current priority value is control, and if it is 
@findCtrlValue:
stz VERA_addr_bank ;Disable auto increment, we need to manually increment the vera to jump up one line if it isn't. The first value that is not control is ultimately returned by this function
lda #LOWEST_BOUNDARY ;Set a default value of zero, which is a control, and will trigger the search for the next value

@loop:
cmp #CONTROL_LINES + 1 ;Check to see whether we have a priority value, go to control if we have
bcs @return 

clc
lda VERA_addr_low ;Go up one line
adc #PRIORITY_WIDTH
sta VERA_addr_low
bcc @checkBoundsHigh
@highByte:
lda #$0 ;Odd
adc VERA_addr_high
sta VERA_addr_high

@checkBoundsHigh: ;If we have out of bounds (upper bounds of course) return #NOT_AN_OBSTACLE, the lowest priority value, otherwise go to get the next value
lda #>PRIORITY_END
cmp VERA_addr_high
beq @checkBoundsLower
bcs @getValue
bra @outOfBounds

@checkBoundsLower:
lda #<PRIORITY_END 
cmp VERA_addr_low
bcs @getValue

@outOfBounds:
lda #NOT_AN_OBSTACLE
bra @return

@getValue:
cpy #$0 ;If we are going forward, then that means we will read the even nibble otherwise the odd nibble
beq @getOddValue

@getEvenValue:
lda VERA_data1
lsr
lsr
lsr
lsr
bra @loop

@getOddValue:
lda VERA_data1
and #$F
bra @loop

@return:
plx ;Restore VERA
stx VERA_addr_bank
plx
stx VERA_addr_high
plx
stx VERA_addr_low
rts

;bECellToVeraBulk(SpriteAttributeSize allocationWidth, SpriteAttributeSize allocationHeight, byte noCels, byte maxVeraSlots, byte xVal, byte yVal, byte pNum);
_bEToBlitCelArray: .res 500
_bECellToVeraBulk:
sta P_NUM

lda RAM_BANK
sta CEL_BANK

jsr popax
sta Y_VAL
stx X_VAL

jsr popa
sta MAX_SPRITE_SLOTS

jsr popa
sta NO_OF_CELS
lda #NO_MARGIN
sta BCOL
stz BULK_ADDRESS_INDEX
stz CEL_COUNTER

lda _sizeofCel
sta SIZE_OF_CEL

lda #<_bEToBlitCelArray
sta CEL_ADDR
lda #>_bEToBlitCelArray
sta CEL_ADDR + 1

;Height:
jsr popa
cmp #SPR_ATTR_8

bne @check16H

@setWidth8H:
lda #SPRITE_TOTAL_ROWS_8
sta TOTAL_ROWS

bra @setWidth

@check16H:
cmp #SPR_ATTR_16
bne @check32H

@setWidth16H:
lda #SPRITE_TOTAL_ROWS_16
sta TOTAL_ROWS
bra @setWidth

@check32H:
cmp #SPR_ATTR_32
bne @setWidth64H

@setWidth32H:
lda #SPRITE_TOTAL_ROWS_32
sta TOTAL_ROWS
bra @setWidth

@setWidth64H:
lda #SPRITE_TOTAL_ROWS_64
sta TOTAL_ROWS
bra @setWidth

;Width
@setWidth:
jsr popa
cmp #SPR_ATTR_8

bne @check16W

@setWidth8W:
lda #BYTES_PER_ROW_8
sta BYTES_PER_ROW

bra @loop

@check16W:
cmp #SPR_ATTR_16
bne @check32W

@setWidth16W:
lda #BYTES_PER_ROW_16
sta BYTES_PER_ROW
bra @loop

@check32W:
cmp #SPR_ATTR_32
bne @setWidth64W

@setWidth32W:
lda #BYTES_PER_ROW_32
sta BYTES_PER_ROW
bra @loop

@setWidth64W:
lda #BYTES_PER_ROW_64
sta BYTES_PER_ROW
bra @loop

@loop:
lda #$1
sta SPLIT_COUNTER

GET_STRUCT_16_STORED_OFFSET _offsetOfSplitCelPointers, CEL_ADDR, SPLIT_CEL_SEGMENTS
GET_STRUCT_8_STORED_OFFSET _offsetOfSplitCelBank, CEL_ADDR, SPLIT_CEL_BANK
GET_STRUCT_8_STORED_OFFSET _offsetOfFlipped, CEL_ADDR, CEL_FLIPPED
GET_STRUCT_8_STORED_OFFSET _offsetOfCelWidth, CEL_ADDR, CEL_WIDTH

lda SPLIT_CEL_SEGMENTS
ora SPLIT_CEL_SEGMENTS + 1

beq @splitLoop ;If the cel is not split we don't need this copy

lda #< bESplitAddressesBuffer
ldx #> bESplitAddressesBuffer
jsr pushax

lda #SPRITE_UPDATES_BANK
jsr pusha

lda SPLIT_CEL_SEGMENTS
ldx SPLIT_CEL_SEGMENTS + 1
jsr pushax

lda SPLIT_CEL_BANK
jsr pusha

lda #POINTER_TO_SPLIT_DATA_SIZE
ldx #$0

jsr _memCpyBankedBetween

@splitLoop: ;Will always iterate once if the cell is not split
GET_STRUCT_8_STORED_OFFSET _offsetOfSplitSegments, CEL_ADDR, SPLIT_SEGMENTS

ldy BULK_ADDRESS_INDEX

stz VERA_ADDRESS ; Low byte Always zero
lda _bEBulkAllocatedAddresses, y ;Middle byte
sta VERA_ADDRESS + 1
lda _bEBulkAllocatedAddresses + 1, y
sta VERA_ADDRESS_HIGH

iny
iny
sty BULK_ADDRESS_INDEX

GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, CEL_ADDR, CEL_TRANS

CLEAR_VERA VERA_ADDRESS, TOTAL_ROWS, BYTES_PER_ROW, #$0

lda Y_VAL
pha

ldx CEL_FLIPPED
bne @celToVeraBackwards

@celToVeraForwards:
lda #$1
sta CEL_TO_VERA_IS_FORWARD_DIRECTION
jsr celToVera
bra @restoreStack

@celToVeraBackwards:
jsr bECelToVeraBackwards

@restoreStack:
pla
sta Y_VAL

@checkSplitLoop:
lda SPLIT_COUNTER
cmp SPLIT_SEGMENTS

bcs @getNextCel
inc SPLIT_COUNTER
jmp @splitLoop

@getNextCel:
clc
lda CEL_ADDR
adc SIZE_OF_CEL
sta CEL_ADDR
lda CEL_ADDR + 1
adc #$0
sta CEL_ADDR + 1


inc CEL_COUNTER

lda MAX_SPRITE_SLOTS
cmp #$1 
beq @checkLoop ;Skip multiply where this view is not split, for efficiency 
ldx #$0
jsr pushax
lda CEL_COUNTER
ldx #$0
TRAMPOLINE #HELPERS_BANK, _b5Multiply
asl
sta BULK_ADDRESS_INDEX

@checkLoop:
dec NO_OF_CELS
beq @return
jmp @loop

@return:
rts


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

newIncrementBackwards: .byte %1000, %1000, %1000, %1000, %1000, %1000, %1000, %1000  ; 8 zeros
    .byte %1000, %11000, %1000, %1000, %1000, %1000, %1000, %1000 ; 8 more zeros
    .byte %1000, %1000, %1000, %1000, %1000, %1000, %1000, %1000 ;8 more zeros
    .byte %11000, %1000  
;Writes a cel to the Vera. The cel must be preloaded at the localCel pointer
;The view (and by extension the cel) must preloaded.
;This view data on the cel is run encoded eg. AX (X instances of colour A)
;Each line is terminated with a 0
;This function is priority screen aware, each pixel will show through according to the following table

;Pixel Priority                  | Result
; 3 (Water)                      | Show through
; 4 (Lowest Priority Background) | Show through
; > 4                            | Show if the sprite priority >= the priority screen pixel at x,y
; < 3 (Control Line)             | Search for nearest upward priority pixel using bEFindPriorityFromCtrlLine and search again


;Note, as each priority byte on the priority screen stores two pixels, the auto increment bit is
;alternated after every read
;The function stops when it has counted height number of zeros, or the line would not be in the drawable area, vertically.
;Note: This function features self modifying code. It is modified by two functions:
;1. bECelToVeraBackwards
;2. b9CelToVera
;These functions restore this function back to original state after completion. 
;The points of self modification are marked with self modified 1, 2 or 1 & 2, along with what the line is self modified to 
;Important Zero Pointers
;Must Be Set Before Calling
;CEL_BANK: Data bank where the cel object is stored
;CEL_ADDR: Address where cel object is stored
;VERA_ADDRESS and VERA_ADDRESS_HIGH: The VERA address of the sprites allocated memory
;X_VAL: The X coord of the sprite (Note AGI blit doubles this value because each pixel is double width)
;Y_VAL: The Y coord of the sprite (Note AGI deducts the height of the sprite as the Y is from the bottom left hand )
;P_NUM: The priority of the sprite being drawn
;CEL_HEIGHT: The height of the cel
;CEL_TO_VERA_IS_FORWARD_DIRECTION: Boolean, Is Operating In Forward Mode 

;Must Be Set For Split Sprites:
;SPLIT_CEL_BANK: Bank where the split bitmaps are stored
;SPLIT_CEL_SEGMENTS: Points to a data structure which begins with a list of addresses of the split bitmaps, and is followed by the split bitmaps themselves
;SPLIT_COUNTER: Which split bitmap are we up to drawing

;Set by this function:
;NEXT_DATA_INDEX: Used as an index into the run encoded data
;BMP_DATA: Used to hold the run encoded bitmap data. Can be either the split data if split or the cel bitmap data otherwise
;BMP_BANK: Bank for the above, can be updated to equal split bank if split
;CEL_TRANS: Cel transparent color
;COLOR: The current current being drawn doubled up (eg. FF instead of F due to pixel doubling)
;NEXT_DATA_INDEX: Index into the BMP_DATA, starts are zero, and incremented after each read. BMP_DATA is incremented by 255 when this resets
celToVera:
stz NEXT_DATA_INDEX

lda RAM_BANK
pha

lda CEL_BANK
sta RAM_BANK

GET_STRUCT_8_STORED_OFFSET _offsetOfCelHeight, CEL_ADDR, CEL_HEIGHT ;Load required attributes from cel
GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, CEL_ADDR, CEL_TRANS

lda SPLIT_SEGMENTS ;If the cel is non split then we get the cel data from the bitmap
cmp #$1
bne celToVeraLowRam_split

celToVeraLowRam_nonSplit:
GET_STRUCT_16_STORED_OFFSET _offsetOfBmp, CEL_ADDR, BMP_DATA  
GET_STRUCT_8_STORED_OFFSET _offsetOfBmpBank, CEL_ADDR, BMP_BANK
bra celToVeraLowRam_start

celToVeraLowRam_split:
lda SPLIT_COUNTER ;The counter would have being set by the calling method
dec
asl

tay

lda SPLIT_CEL_BANK
sta BMP_BANK
sta RAM_BANK

lda (SPLIT_CEL_SEGMENTS), y ;Otherwise we get it from the split segments
sta BMP_DATA 
iny
lda (SPLIT_CEL_SEGMENTS), y
sta BMP_DATA + 1

celToVeraLowRam_start:
lda CEL_TRANS
SET_COLOR_LEFT CEL_TRANS ;Double up the color (eg turn 0xF into 0xFF as agi pixels are doubled)
sta CEL_TRANS

lda BMP_BANK
sta RAM_BANK

celToVeraLowRam_setVeraAddress: ;Begin plot line/next line
SET_VERA_ADDRESS VERA_ADDRESS, #$1, VERA_ADDRESS_HIGH, #$0 ;Set the vera address of the first point on this line on the sprite

ldy Y_VAL ;Work out the priority address of the first point in this line. Make use of the priority line table
celToVeraLowRam_setPriorityAddress:
VERA_CTRL_SET #$1
lda X_VAL
lsr

celToVeraLowRam_setPriorityAdd: 
clc
celToVeraLowRam_setPriorityAddLow:
adc lineTablePriorityLow,y             ;add low byte of (y << 5) + (y << 7) 
sta VERA_addr_low         ; store low byte result (because 160<0xff) 
celToVeraLowRam_setPriorityAddHigh:
lda lineTablePriorityHigh,y 
adc #$00                   ; add carry
sta VERA_addr_high
celToVeraLowRam_setPrioritySetDirection:
lda #$0 ;self modify 2 to lda #%1000
sta VERA_addr_bank

celToVeraLowRam_calculatePriorityAutoInc: ;This calculates whether the priority auto incrementment should be initially switched on or not. If X even auto inc should be off, because we need to read the same byte for the next turn
lda X_VAL
lsr
bcs celToVeraLowRam_oddValue

;For forward direction if we are on an even nibble, don't auto increment as we need to read the odd nibble of the same byte next. This is the opposite for backwards
celToVeraLowRam_evenValue:
lda #$0 ;Can't using stz here, as self modifying code is using to change the value ;Self Modify 2 to lda #%11000
sta VERA_addr_bank ;Disable, we need to read this byte again
bra celToVeraLowRam_getNextChunk

celToVeraLowRam_oddValue:
lda #%10000 ;Odd on the next read we should move onto the next byte ;Self Modify 2 to #%1000
sta VERA_addr_bank

celToVeraLowRam_getNextChunk:
READ_NEXT_BYTE ;Read the next run encoded byte
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

cmp CEL_TRANS ;We can skip drawing if the pixel color is equal to the transparent color
bne celToVeraLowRam_draw

celToVeraLowRam_skip: ;Skip a certain number of transparent pixels, which where is is more than about 4 in a row, is faster than just iterating 'number of transparant pixel' times
celToVeraLowRam_spriteMemoryAdd:
stz VERA_ctrl ;Skipping visual pixels is easy, just add the skip amount
tya
clc
adc VERA_addr_low
sta VERA_addr_low

bcc celToVeraLowRam_prepareForPriorityAdd ;Skipping high byte, if not needed

lda #$0
adc VERA_addr_high
sta VERA_addr_high

celToVeraLowRam_prepareForPriorityAdd: ;Skipping priority is much harder. We need to add half of the skip value, but we may also need to turn on or off the increment, and/or add an additional 1. Whether we do hinges on two values the partity of the amount being skipped (even or odd) and whether the increment on initially.
;There are tables in an Excel file called tableForAdder at the route of the repo, which indicates the action. 
;Why we might need to take away an additional amount, this happens only if we start with increment on, and take away an odd amount.
;If we half an odd skip value, it rounds down. Therefore is the increment is turned on initially we need to add an additional 1. 
;Say for example the increment is on and the skip value is three. We need to add two, the 1 from the halving of three plus the additional 1 from the increment value.
;See the table in  'tableForAdder.xls'
;As for whether we need to turn on the increment or not. We use a simple rule (odd + odd = even (increment off)) (odd + even  = odd increment off) (even + even = even (increment on))
lda #$1 ;
sta VERA_ctrl

tya ;Form a number which we will use to determine whether amount to add is even or odd (bit 0) and whether the incrementor is initially on (bit 5). If they are both on that is 0x11 or 17

celToVeraLowRam_prepareForPriorityAnd:
and #1
ora VERA_addr_bank
tax
celToVeraLowRam_prepareForPriorityCmp:
cmp #%10001 ;Self Modify 2 to #%11001
bne celToVeraLowRam_noExtraAddRequired
celToVeraLowRam_extraAddRequired:
tya
inc
lsr
bra celToVeraLowRam_addPriority
celToVeraLowRam_noExtraAddRequired:
tya
lsr

celToVeraLowRam_addPriority: ;Self Modify 2 To sec
clc
sta CEL_TO_VERA_GENERAL_TMP
lda VERA_addr_low
celToVeraLowRam_addPriorityLow:
adc CEL_TO_VERA_GENERAL_TMP ;Self Modify 2 To sbc zp
sta VERA_addr_low
celToVeraLowRam_branchAfterLowByte:
bcc celToVeraLowRam_determineIncrementValue ;Self Modify 2 to bcs implied
lda VERA_addr_high
celToVeraLowRam_addPriorityHigh:
adc #$0 ;Self Modify 2 to sbc immediate
sta VERA_addr_high

celToVeraLowRam_determineIncrementValue:
lda newIncrementValue,x ;Self modify 2 to #newIncrementBackwards
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

celToVeraLowRam_checkPriorityCmp:
cmp #$0; Self modify 2 to #%11000. We need the cmp #$0 as we need a placeholder for replacement

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
cmp #WATER ;The object always has precedence over water
beq celToVeraLowRam_drawColor
bcs celToVeraLowRam_cmpPNum ;If the priority screen has a 1 or a 2 then that means that are on a control line, we need to work out the priority from neighbouring pixels. bEFindPriorityFromCtrlLine does that job.
celToVeraLowRam_FindPriorityFromCtrlLine:
lda RAM_BANK
pha
phy 

lda #SPRITE_UPDATES_BANK
sta RAM_BANK
jsr bEFindPriorityFromCtrlLine
ply
plx 
stx RAM_BANK
bra celToVeraLowRam_comparePriority

celToVeraLowRam_cmpPNum:
cmp P_NUM
beq celToVeraLowRam_drawColor ;If the object and screen priority is equal the object has precedence
bcc celToVeraLowRam_drawColor ;If the object screen priority < object priority the object has precedence

celToVeraLowRam_skipBasedOnPriority: ;This means that the screen priority > object priority, so we don't draw
stz VERA_data0 ;Self modify 1 to ldx
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
lda BYTES_PER_ROW  ;Work out VERA address for the next line of the sprite
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
