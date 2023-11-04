
.ifndef VIEW_INC
; Set the value of include guard and define constants
VIEW_INC = 1
;DEBUG_VIEW_DRAW = 1

.include "globalViews.s"
.include "global.s"
.include "pictureAsm.s"

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

;byte* localCel, long veraAddress, byte pNum, byte bCol, byte drawingAreaWidth
VERA_BYTES_PER_ROW = ZP_TMP
BCOL = ZP_TMP_2
PNUM = ZP_TMP_3
VERA_ADDRESS = ZP_TMP_4
VERA_ADDRESS_HIGH = ZP_TMP_5
LOCAL_CEL = ZP_TMP_6
BUFFER_STATUS = ZP_TMP_7 ;Takes Up 8 as well
BUFFER_POINTER = ZP_TMP_9
CEL_HEIGHT = ZP_TMP_10
CEL_TRANS = ZP_TMP_12
COLOUR = ZP_TMP_13
CEL_BMP_OFFSET = 3
CEL_BANK_OFFSET = 5
CEL_HEIGHT_OFFSET = 1
CEL_TRANS_OFFSET = 2


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

_b9ViewToVera:
sta BYTES_PER_ROW
jsr popax
sta BCOL
stx PNUM
jsr popax
sta VERA_ADDRESS
stx VERA_ADDRESS + 1
jsr popax
sta VERA_ADDRESS_HIGH
stx VERA_ADDRESS_HIGH + 1
jsr popax
sta LOCAL_CEL
stx LOCAL_CEL + 1

GET_STRUCT_16 CEL_BMP_OFFSET, LOCAL_CEL, BUFFER_STATUS
GET_STRUCT_8 CEL_BANK_OFFSET, LOCAL_CEL, BUFFER_STATUS + 2
GET_STRUCT_8 CEL_HEIGHT_OFFSET, LOCAL_CEL, CEL_HEIGHT
GET_STRUCT_8 CEL_TRANS_OFFSET, LOCAL_CEL, CEL_TRANS

stz BUFFER_STATUS + 3

REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS

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
and #$0F
tay
txa
and #$F0
sta COLOUR
SET_COLOR COLOUR

cmp CEL_TRANS
bne @draw

@skip:
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
lda BYTES_PER_ROW
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
rts
.endif


