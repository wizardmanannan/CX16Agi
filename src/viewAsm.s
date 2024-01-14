
.ifndef VIEW_INC
; Set the value of include guard and define constants
VIEW_INC = 1
;DEBUG_VIEW_DRAW = 1

.include "globalViews.s"
.include "global.s"
.include "pictureAsm.s"
.include "spriteMemoryManagerAsm.s"

.import _b5RefreshBuffer
.import _getLoadedView
.import popax
.import popa
.import pushax
.import _memCpyBanked
.import _b1DivAndCeil

.import _offsetOfBmp
.import _offsetOfBmpBank
.import _offsetOfCelHeight
.import _offsetOfCelTrans
.import _offsetOfSplitCelBank
.import _offsetOfCelWidth
.import _offsetOfCelHeight
.import _offsetOfSplitCelPointers
.import _offsetOfSplitSegments


.import _sizeofCel

.import _b10BankedAlloc
.import _b5Multiply
.import _b5Divide

.ifdef DEBUG_VIEW_DRAW
.import _b5PrintChunk
.endif

.segment "BANKRAM09"
_viewHeaderBuffer: .res VIEW_HEADER_BUFFER_SIZE
_loopHeaderBuffer: .res LOOP_HEADER_BUFFER_SIZE

;Color must be loaded into A
.macro SET_COLOR_LEFT TMP
asl a           ; Shift left 4 times to multiply by 16
asl a  
asl a  
asl a  
ora TMP
.endmacro

;Color must be loaded into A
.macro SET_COLOR_RIGHT TMP
lsr a           ; Shift left 4 times to multiply by 16
lsr a  
lsr a  
lsr a  
ora TMP
.endmacro

.macro DEBUG_VIEW_DRAW
.ifdef DEBUG_VIEW_DRAW
sta _logDebugVal1
pha
txa
pha
tya
pha

JSRFAR _b5PrintChunk, DEBUG_BANK

pla
tay
pla
tax
pla
.endif
.endmacro

;Used in both single and bulk
VERA_BYTES_PER_ROW = ZP_TMP_2
BCOL = ZP_TMP_3
VERA_ADDRESS = ZP_TMP_4
VERA_ADDRESS_HIGH = ZP_TMP_5
LOCAL_CEL = ZP_TMP_6
BUFFER_STATUS = ZP_TMP_7 ;Takes Up 8 as well
BUFFER_POINTER = ZP_TMP_9
CEL_HEIGHT = ZP_TMP_10
CEL_TRANS = ZP_TMP_12
OUTPUT_COLOUR = ZP_TMP_13

;Constants
CEL_HEIGHT_OFFSET = 1
CEL_TRANS_OFFSET = 2
NO_MARGIN = 4
;byte* localCel, long veraAddress, byte bCol, byte drawingAreaWidth
;Writes a cel to the Vera. The cel must be preloaded at the localCel pointer
;The view (and by extension the cel) must preloaded.
;This view data on the cel is run encoded eg. AX (X instances of colour A)
;Each line is terminated with a 0
;The function stops when it has counted height number of zeros

.macro CEL_TO_VERA
GET_STRUCT_16_STORED_OFFSET _offsetOfBmp, LOCAL_CEL, BUFFER_STATUS ;Buffer status holds the C struct to be passed to b5RefreshBuffer
GET_STRUCT_8_STORED_OFFSET _offsetOfBmpBank, LOCAL_CEL, BUFFER_STATUS + 2
GET_STRUCT_8_STORED_OFFSET _offsetOfCelHeight, LOCAL_CEL, CEL_HEIGHT
GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, LOCAL_CEL, CEL_TRANS

stz BUFFER_STATUS + 3

REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS ;Uses the work area to buffer the run encoded data. Using the b5RefreshBuffer function from C

.ifdef DEBUG_VIEW_DRAW
stz _logDebugVal1
.endif

lda CEL_TRANS
SET_COLOR_LEFT CEL_TRANS
sta CEL_TRANS

@setVeraAddress:
SET_VERA_ADDRESS VERA_ADDRESS, #$1, VERA_ADDRESS_HIGH

@getNextChunk:
GET_NEXT BUFFER_POINTER, BUFFER_STATUS
DEBUG_VIEW_DRAW

cmp #$0
beq @increment
tax
and #$0F; The number of pixels is the lower 4 bits
tay
txa
and #$F0; The colour is the upper 4 bits
sta OUTPUT_COLOUR
SET_COLOR_RIGHT OUTPUT_COLOUR

cmp CEL_TRANS
bne @draw

@skip: ;When 'drawing' transparent pixels we still need to increment the address
ldx VERA_data0 ;We are not changing this one so we load load in order to increment and ignore the value
dey
beq @getNextChunk
bra @skip

@draw:
@drawBlack:
cmp #BLACK_COLOR
bne @drawColor
lda CEL_TRANS ;Black is swapped with the transparent colour in the case the transparent colour is not black

@drawColor:
sta VERA_data0

@decrementColorCounter:
dey
beq @getNextChunk
bra @draw

@increment:
dec CEL_HEIGHT
beq @end

clc
lda BYTES_PER_ROW ;Ready for the next line
adc VERA_ADDRESS
sta VERA_ADDRESS
lda #$0
adc VERA_ADDRESS + 1
sta VERA_ADDRESS + 1
lda #$0
adc VERA_ADDRESS_HIGH
sta VERA_ADDRESS_HIGH

jmp @setVeraAddress

@end:
.endmacro

;byte* localCel, long veraAddress, byte pNum, byte bCol, byte drawingAreaWidth
_b9CelToVera:
sta BYTES_PER_ROW
jsr popa
sta BCOL
jsr popax
sta VERA_ADDRESS
stx VERA_ADDRESS + 1
jsr popax
sta VERA_ADDRESS_HIGH
stx VERA_ADDRESS_HIGH + 1
jsr popax
sta LOCAL_CEL
stx LOCAL_CEL + 1

CEL_TO_VERA

rts

;Used in bulk
NO_TO_BLIT = ZP_TMP_14
BULK_ADDRESS_INDEX = ZP_TMP_16
SIZE_OF_CEL = ZP_TMP_17
CLEAR_COLOR = ZP_TMP_18
TOTAL_ROWS = ZP_TMP_19
.segment "BANKRAM0E"
;byte allocationSize, byte noToBlit Note 32 is 0 allocation size and 64 is 1
_bEToBlitCelArray: .res 500
_bECellToVeraBulk:
sta NO_TO_BLIT
lda #NO_MARGIN
sta BCOL
stz BULK_ADDRESS_INDEX

lda _sizeofCel
sta SIZE_OF_CEL

lda #<_bEToBlitCelArray
sta LOCAL_CEL
lda #>_bEToBlitCelArray
sta LOCAL_CEL + 1

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
ldy BULK_ADDRESS_INDEX

stz VERA_ADDRESS ; Low byte Always zero
lda _bEBulkAllocatedAddresses, y ;Middle byte
sta VERA_ADDRESS + 1
lda _bEBulkAllocatedAddresses + 1, y
sta VERA_ADDRESS_HIGH

iny
iny
sty BULK_ADDRESS_INDEX

GET_STRUCT_8_STORED_OFFSET _offsetOfCelTrans, LOCAL_CEL, CEL_TRANS

CLEAR_VERA VERA_ADDRESS, TOTAL_ROWS, BYTES_PER_ROW, #$0

CEL_TO_VERA

clc
lda LOCAL_CEL
adc SIZE_OF_CEL
sta LOCAL_CEL
lda LOCAL_CEL + 1
adc #$0
sta LOCAL_CEL + 1

@checkLoop:
dec NO_TO_BLIT
beq @return
jmp @loop

@return:
rts

SPLIT_CEL_WIDTH = ZP_TMP_3
SPLIT_CEL_HEIGHT = ZP_TMP_3 + 1
SPLIT_BANK = ZP_TMP_4
CEL_DATA_BANK = ZP_TMP_5
SPLIT_BUFFER_STATUS = ZP_TMP_6 ;Takes Up 7 as well
NO_BYTES_SIZE = ZP_TMP_8
SPLIT_BUFFER_POINTER = ZP_TMP_9
SPLIT_DATA = ZP_TMP_10
CEL_DATA = ZP_TMP_12
SEGMENT_SIZE = ZP_TMP_13
NO_SEGMENTS = ZP_TMP_13 + 1
CEL_STRUCT_POINTER = ZP_TMP_16
;14 and 15 used for temp storage here


.macro PARTITION_MEMORY
.local @endDivideSegments
.local @createSegmentPointersLoop

ldy #$0
ldx NO_SEGMENTS ;x is serving as our counter
@createSegmentPointersLoop: ;This is for partioning memory into segments. It assumes that the first address in already store in ZP_TMP_14
lda ZP_TMP_14
sta GOLDEN_RAM_WORK_AREA, y ; Store the address in the golden ram work area
iny
lda ZP_TMP_14 + 1
sta GOLDEN_RAM_WORK_AREA, y
iny

dex
cpx #$0
beq @endDivideSegments

clc
lda SEGMENT_SIZE ;Work out the next address by adding segment_size to the previous address
adc ZP_TMP_14
sta ZP_TMP_14
lda ZP_TMP_14 + 1
adc #$0
sta ZP_TMP_14 + 1
bra @createSegmentPointersLoop

@endDivideSegments:
.endmacro

MAX_SPRITES_ROW_OR_COLUMN_SIZE = 4
POINTER_TO_SPLIT_DATA_SIZE = MAX_SPRITES_ROW_OR_COLUMN_SIZE * MAX_SPRITES_ROW_OR_COLUMN_SIZE * 2
.macro PREPARE_BUFFER_SPLIT_CEL
stz SPLIT_BUFFER_STATUS + 3

lda CEL_DATA
sta SPLIT_BUFFER_STATUS
lda CEL_DATA + 1
sta SPLIT_BUFFER_STATUS + 1

lda CEL_DATA_BANK
sta SPLIT_BUFFER_STATUS + 2

REFRESH_BUFFER SPLIT_BUFFER_POINTER, SPLIT_BUFFER_STATUS ;Uses the work area to buffer the run encoded data. Using the b5RefreshBuffer function from C

.endmacro
;void bESplitCel(Cel* cel);
_bESplitCel: ;The goal of this function is to PREPARE to split a cel into segments that can be drawn to the vera. The segments are stored in banked memory, and the pointers to the segments are stored in the cel struct
;There are two split cel methods, this one for preparation and bCSplitCel for spliting.
;This method has several steps, in this order.
; 1. Read the cel struct to get the width, height, and cel data address and bank
; 2. Count the number of bytes in the cel data, so we know how much to allocate
; 3. Allocate memory for the split cel data
; 4. Divide the memory by the number of segments
; 5. Store those divisions prior calling split
; 6. Now partition the bCSplitBuffer buffer into equal sized segments to the allocated memory. Note even though this buffer is larger then the allocated memory, any excess space will be discounted. This allows for the pointers to both buffers to point to the same data eg. the first pointer in the allocated memory will point to the first segment in the buffer will point the same data as the first pointer for the buffer and so on
; 7. Store the pointers to the divisions of bCSplitBuffer buffer in bCSplitBufferSegments
; 8. Call bCSplitCel
; 9. Copy the data from bCSplitBuffer to the allocated memory

sta CEL_STRUCT_POINTER
stx CEL_STRUCT_POINTER + 1

;1. Read cel struct data

GET_STRUCT_16_STORED_OFFSET _offsetOfBmp, CEL_STRUCT_POINTER, CEL_DATA
GET_STRUCT_8_STORED_OFFSET _offsetOfCelWidth, CEL_STRUCT_POINTER, SPLIT_CEL_WIDTH
GET_STRUCT_8_STORED_OFFSET _offsetOfCelHeight, CEL_STRUCT_POINTER, SPLIT_CEL_HEIGHT
GET_STRUCT_8_STORED_OFFSET _offsetOfBmpBank, CEL_STRUCT_POINTER, CEL_DATA_BANK

 ;2. Count the number of bytes
PREPARE_BUFFER_SPLIT_CEL ;This will set the buffering mechanism to the start of the cel data
ldy SPLIT_CEL_HEIGHT

stz NO_BYTES_SIZE ;Count number of bytes in the cel data
stz NO_BYTES_SIZE + 1
ldx #$0
@loopStart:
GET_NEXT SPLIT_BUFFER_POINTER, SPLIT_BUFFER_STATUS ;Retrieves the next byte from the buffer
inx
bne @countCheckNextLine
inc NO_BYTES_SIZE + 1
@countCheckNextLine:
cmp #$0 ;Zero means a new line, but we need to still count it, and then decrement y
bne @loopStart

dey ;We stop when we have counted every line
bne @loopStart

@endCount:
stx NO_BYTES_SIZE

;Allow enough space for extra line terminators when we split the cel horizontally. There are a maximum of 4 extra terminators (max split is 4) per line
clc ; Height * 4 equals maximum number of extra horizontal terminators
lda SPLIT_CEL_HEIGHT 
asl
sta ZP_TMP_14
lda #$0
rol
sta ZP_TMP_14 + 1
clc
lda ZP_TMP_14
asl
sta ZP_TMP_14
lda #$0
rol
sta ZP_TMP_14 + 1

;Double that value, because when we split there will be an extra byte. This will be for whatever couldn't fit in the first segment per line
clc
lda ZP_TMP_14
asl
sta ZP_TMP_14
lda ZP_TMP_14 + 1
rol
sta ZP_TMP_14 + 1

;Add this doubled value to the number of bytes count
clc 
lda ZP_TMP_14
adc NO_BYTES_SIZE
sta NO_BYTES_SIZE
lda ZP_TMP_14 + 1
adc NO_BYTES_SIZE + 1
sta NO_BYTES_SIZE + 1

;Add the vertical terminators there can be a maximum of 4, since we split vertically a maximum of 4 times
;Also add enough space for MAX_SPRITES_ROW_OR_COLUMN_SIZE * MAX_SPRITES_ROW_OR_COLUMN_SIZE pointers (which are two bytes each) at the very start
clc
lda NO_BYTES_SIZE
adc #POINTER_TO_SPLIT_DATA_SIZE + MAX_SPRITES_ROW_OR_COLUMN_SIZE
sta NO_BYTES_SIZE
lda #$0
adc NO_BYTES_SIZE + 1
sta NO_BYTES_SIZE + 1

; 3. Allocate memory

lda NO_BYTES_SIZE
ldx NO_BYTES_SIZE + 1
jsr pushax

lda #< SPLIT_BANK
ldx #$0
TRAMPOLINE #BANKED_ALLOC_BANK, _b10BankedAlloc
sta SPLIT_DATA
stx SPLIT_DATA + 1
SET_STRUCT_16_STORED_OFFSET_VALUE_IN_REG _offsetOfSplitCelPointers, CEL_STRUCT_POINTER

lda SPLIT_BANK
SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfSplitCelBank, CEL_STRUCT_POINTER

; 4. Divide the memory by the number of segments
lda SPLIT_CEL_WIDTH ;Divide width by 64 to know how many segments across we need
cmp #64
bcc @widthLessThan64
clc
adc #63 ; Adding 63 to the width will ensure we round up when we divide by 64. This is an optimisation trick that saves us calling the C float division function
lsr
lsr
lsr
lsr
lsr
lsr 
bra @loadWidth

@widthLessThan64:
lda #$1 ;If width is less than 64 we only need one segment

@loadWidth:
ldx #$0; High byte is always zero
sta ZP_TMP_14
stx ZP_TMP_14 + 1

lda SPLIT_CEL_HEIGHT
cmp #64
bcc @heightLessThan64
clc
adc #63 ; Adding 63 to the height will ensure we round up when we divide by 64
lsr
lsr
lsr
lsr
lsr
lsr 
bra @loadHeight

@heightLessThan64:
lda #$1 ;If height is less than 64 we only need one segment

@loadHeight:
ldx #$0 ;High byte is always zero

cmp #1
beq @storeSegmentSize ;Height is 1 we can skip the multiply as we would just be multiply width by 1. We already have width in ZP_TMP_14

ldy #$1
cpy ZP_TMP_14 ;If width is 1 we can also skip the multiply, as we would just be multiplying height by 1. Note if both values were 1 we would never have gotten into the split function in the first place

bne @multiply
beq @storeMultiplyResult


@multiply:
jsr pushax ;We already have the height in a and x
lda ZP_TMP_14 ; Get Width
ldx #$0
TRAMPOLINE #HELPERS_BANK, _b5Multiply

@storeMultiplyResult:
sta ZP_TMP_14

@storeSegmentSize: 
lda ZP_TMP_14
sta NO_SEGMENTS

@divideMemoryBySegments: ;Now we know width and height we can divide the memory by the number of segments to get the segment size
sec
lda NO_BYTES_SIZE ;We don't count the part of the memory reserved for the pointers in the segment division
sbc POINTER_TO_SPLIT_DATA_SIZE
tay ;Store the low byte in y as we will need this when we call divide, but we need a to do the high byte of the subtraction
lda NO_BYTES_SIZE + 1
sbc POINTER_TO_SPLIT_DATA_SIZE + 1
tax ;The high byte must be in x prior to the call to the division function
tya ;The low byte must be in a prior to the call to the division function
jsr pushax

lda NO_SEGMENTS
ldx #$0

SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfSplitSegments, CEL_STRUCT_POINTER

TRAMPOLINE #HELPERS_BANK, _b5Divide ;Divide the total amount of memory by the number of segments
sta SEGMENT_SIZE

lda SPLIT_DATA 
sta ZP_TMP_14
lda SPLIT_DATA + 1
sta ZP_TMP_14 + 1

PARTITION_MEMORY ;Now partition the memory into segments. Store initially in the golden ram and then copy to the banked memory

;5. Store those divisions
lda SPLIT_DATA
ldx SPLIT_DATA + 1
jsr pushax

lda #<GOLDEN_RAM_WORK_AREA
ldx #>GOLDEN_RAM_WORK_AREA
jsr pushax

lda SPLIT_BANK
jsr pusha

lda NO_SEGMENTS
asl
ldx #$0
jsr _memCpyBanked

;6. Now partition the bCSplitBuffer buffer into equal sized segments to the allocated memory
lda #<bCSplitBuffer 
sta ZP_TMP_14
lda #>bCSplitBuffer 
sta ZP_TMP_14 + 1

PARTITION_MEMORY

;7. Store the pointers to the divisions of bCSplitBuffer buffer
lda #< bCSplitBufferSegments
ldx #> bCSplitBufferSegments
jsr pushax

lda #<GOLDEN_RAM_WORK_AREA
ldx #>GOLDEN_RAM_WORK_AREA
jsr pushax

lda #SPLIT_BUFFER_BANK
jsr pusha

lda NO_SEGMENTS
asl
ldx #$0
jsr _memCpyBanked

lda #<bCSplitBuffer 
sta ZP_TMP_14
lda #>bCSplitBuffer 
sta ZP_TMP_14 + 1

;8. Call bCSplitCel
PREPARE_BUFFER_SPLIT_CEL ;While in a perfect world this would live in bCSplitCel, it is here because there isn't much room left on C
TRAMPOLINE #SPLIT_BUFFER_BANK, _bCSplitCel

; 9. Copy the data from bCSplitBuffer to the allocated memory
;TODO

@end:
rts
.endif

.segment "BANKRAM0C"
bCSplitBuffer: .res 5000
bCSplitBufferSegments: .res 32

_bCSplitCel: ;Must be called by bESplitCel, which does all of the prepartion, as this depends on the data in the zero page values set up by bESplitCel and the buffer prepare macro being called

@heightLoop

rts