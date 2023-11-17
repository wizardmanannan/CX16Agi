
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

.ifdef DEBUG_VIEW_DRAW
.import _b5PrintChunk
.endif

.segment "BANKRAM09"
_viewHeaderBuffer: .res VIEW_HEADER_BUFFER_SIZE
_loopHeaderBuffer: .res LOOP_HEADER_BUFFER_SIZE
_spritesUpdatedBuffer: .res VIEW_TABLE_SIZE

.macro SET_COLOR COLOUR
lsr a           ; Shift left 4 times to multiply by 16
lsr a  
lsr a  
lsr a  
ora COLOUR
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
COLOUR = ZP_TMP_13

;Constants
CEL_BMP_OFFSET = 3
CEL_BANK_OFFSET = 5
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
GET_STRUCT_16 CEL_BMP_OFFSET, LOCAL_CEL, BUFFER_STATUS ;Buffer status holds the C struct to be passed to b5RefreshBuffer
GET_STRUCT_8 CEL_BANK_OFFSET, LOCAL_CEL, BUFFER_STATUS + 2
GET_STRUCT_8 CEL_HEIGHT_OFFSET, LOCAL_CEL, CEL_HEIGHT
GET_STRUCT_8 CEL_TRANS_OFFSET, LOCAL_CEL, CEL_TRANS

stz BUFFER_STATUS + 3

REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS ;Uses the work area to buffer the run encoded data. Using the b5RefreshBuffer function from C

.ifdef DEBUG_VIEW_DRAW
stz _logDebugVal1
.endif

lda CEL_TRANS
SET_COLOR CEL_TRANS
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
sta COLOUR
SET_COLOR COLOUR

cmp CEL_TRANS
bne @draw

@skip: ;When 'drawing' transparent pixels we still need to increment the address
ldx VERA_data0 ;We are not changing this one so we load load in order to increment and ignore the value
dey
beq @getNextChunk
bra @skip

@draw:
sta VERA_data0
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
TO_BLIT_ARRAY_INDEX = ZP_TMP_16
BULK_ADDRESS_INDEX = ZP_TMP_17
.segment "BANKRAM0E"
;byte bytesPerRow, byte noToBlit
_bEToBlitArray: .res VIEW_TABLE_SIZE * 2, $0
_bECellToVeraBulk:
sta NO_TO_BLIT
jsr popa
sta BYTES_PER_ROW
lda #NO_MARGIN
sta BCOL
stz TO_BLIT_ARRAY_INDEX
stz BULK_ADDRESS_INDEX

@loop:
ldy TO_BLIT_ARRAY_INDEX
lda _bEToBlitArray, y
sta LOCAL_CEL
lda _bEToBlitArray + 1, y
sta LOCAL_CEL + 1
iny
iny
sty TO_BLIT_ARRAY_INDEX

ldy BULK_ADDRESS_INDEX
lda _bEBulkAllocatedAddresses, y
sta VERA_ADDRESS
lda _bEBulkAllocatedAddresses + 1, y
sta VERA_ADDRESS + 1
lda _bEBulkAllocatedAddresses + 2, y
sta VERA_ADDRESS_HIGH
stz VERA_ADDRESS_HIGH + 1 ;Always zero
iny
iny
iny
sty BULK_ADDRESS_INDEX

CEL_TO_VERA

@checkLoop:
dec NO_TO_BLIT
beq @return
jmp @loop

@return:
rts

.endif


