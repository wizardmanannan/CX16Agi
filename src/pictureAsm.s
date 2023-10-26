; Check if global definitions are included, if not, include them_picColour
.ifndef  PICTURE_INC
PICTURE_INC = 1
;TEST_IS_MULTIPLE_OF_160 = 1 

.include "x16.inc"
.include "global.s"
.include "graphicsAsm.s"
.include "helpersAsm.s"


.import _picColour
.import _picDrawEnabled
.import _priDrawEnabled
.import _noSound

.import _b11FloodBankFull

.import popa
.import popax
.import pushax

.importzp tmp4
.importzp ptr4

.segment "CODE"

;DEBUG_PIXEL_DRAW = 1


.import _b5DebugPixelDraw
.import _b5DebugPrePixelDraw
.import _b5CheckPixelDrawn
.import _b5DebugPixelDrawAddress
.import _stopAtPixel
.import _b5DebugFloodQueueRetrieve
.import _b5DebugFloodQueueStore
.import _pixelCounter
.import _pixelStartPrintingAt
.import _pixelStopAt
.import _queueAction
.import _pixelFreezeAt
.import _isMultipleOf160
.import _exit
.import _stopAtQueueAction

.ifdef DEBUG_CHECK_LINE_DRAWN
.import _b5LineDrawDebug
.endif

MAX_X = 160
MAX_Y = 168

MAX_ADDRESS = $7F7F

ZP_PREMULTIPLY_TABLE = ZP_TMP_20

.macro NEIGHBOURHOODCHECK OK_TO_FILL_LOWER
.local check_goNoFurtherLeft
.local check_goNoFurtherRight
.local continueNCheck
.local storeInQueue
.local endNeighbourHoodCheck
check_goNoFurtherLeft:
cpy #$2 ; Only relevant for check two x - 1 skip overwise
bne check_goNoFurtherRight
lda GO_NO_FURTHER_LEFT
beq continueNCheck
jmp endNeighbourHoodCheck

check_goNoFurtherRight:
cpy #$4

bne continueNCheck
lda GO_NO_FURTHER_RIGHT
beq continueNCheck
jmp endNeighbourHoodCheck

continueNCheck:
lda TO_STORE,y
sta OK_TO_FILL_ADDRESS
iny
lda TO_STORE,y
sta OK_TO_FILL_ADDRESS + 1

OK_TO_FILL_LOWER
bne storeInQueue
jmp endNeighbourHoodCheck
storeInQueue:

ldy #$0
lda OK_TO_FILL_ADDRESS,y
FLOOD_Q_STORE
ldy #$1
lda OK_TO_FILL_ADDRESS,y
FLOOD_Q_STORE
endNeighbourHoodCheck:
.endmacro


.macro STOP_AT_QUEUE_ACTION
pha
txa
pha
tya
pha
jsr _stopAtQueueAction
pla
tay
pla
tax
pla
.endmacro

.macro IS_MULT_OF_160 word
   .local @fail
   .local @end 
   .local @result
   .local @shiftLoop
   .local @lastForMustBeZero
   
   lda word + 1
   bne @lastForMustBeZero ; If the high byte is zero then the value must be greater than 160 and could be a multiple
   lda word
   beq @succeed ; Zero is a factor
   cmp #160
   beq @succeed ; 160 is a factor
   bra @fail ; Less than 256 and not equal to 0 or 160 therefore not a factor

   @lastForMustBeZero:
   lda word
   and #$0F ;Last four bits must be 0
   bne @fail
   

   lda word + 1
   and #$10
   bne @fail ; Because 160 is 16 × 10  number >> 4 must be a multiple of 10. Which means that last bit of the first nibble of the high byte must be even, 
   
   lda word
   ldx word + 1
   jsr _isMultipleOf160
   bra @end

   @fail:
   lda #$0
   bra @end
   @result: .word $10
   @succeed:
   lda #$1
   @end:
.endmacro

.macro PRINT_PIXEL_MESSAGE printFunc
.local @end
EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelStartPrintingAt, @end
JSRFAR printFunc, DEBUG_BANK
@end:
.endmacro

.macro DEBUG_PIXEL_DRAW_ADDRESS address
.ifdef DEBUG_PIXEL_DRAW
lda address
sta _logDebugVal1
lda address + 1
sta _logDebugVal2
PRINT_PIXEL_MESSAGE _b5DebugPixelDrawAddress
.endif
.endmacro

.macro DEBUG_PIXEL_DRAW var1, var2
.ifdef DEBUG_PIXEL_DRAW
lda var1
ldx var2
JSRFAR _b5DebugPixelDrawAsm, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_PREPIXEL_DRAW
.ifdef DEBUG_PIXEL_DRAW

 lda var1
 ldx var2
 JSRFAR _b5DebugPrePixelDrawAsm, DEBUG_BANK
 .endif
.endmacro

.macro DEBUG_PIXEL_DRAWN var1, var2
.ifdef DEBUG_PIXEL_DRAW

lda var1
ldx var2
 JSRFAR _b5DebugPixelDrawnAsm, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_LINE_DRAW var1, var2, var3, var4
.ifdef DEBUG_CHECK_LINE_DRAWN
.local @shouldPrint
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
lda var3
sta _logDebugVal3
lda var4
sta _logDebugVal4
JSRFAR _b5LineDrawDebug, DEBUG_BANK
.endif
.endmacro


.macro DEBUG_FLOOD_QUEUE_RETRIEVE
.local @end

.ifdef DEBUG_PIXEL_DRAW


sta _logDebugVal1
EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelStartPrintingAt, @end
JSRFAR _b5DebugFloodQueueRetrieve, DEBUG_BANK
@end:
INC_32 _queueAction
lda _logDebugVal1
.endif
.endmacro

.macro DEBUG_FLOOD_QUEUE_STORE
.local @increment
.local @queue
.ifdef DEBUG_PIXEL_DRAW
lda @queue
EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @increment
LESS_THAN_32 _pixelCounter, _pixelStartPrintingAt, @increment
lda var1
sta _logDebugVal1
JSRFAR _b5DebugFloodQueueStore, DEBUG_BANK
@increment:
INC_32 _queueAction
bra @end
@queue .byte $0
@end:
.endif
.endmacro

.macro SET_PICCOLOR
lda _picColour
asl a           ; Shift left 4 times to multiply by 16
asl a  
asl a  
asl a  
ora _picColour
sta _toDraw     ; toDraw = picColour << 4 | picColour

.endmacro

_drawWhere: .word $0
_toDraw: .byte $0
.macro PSET coX, coY
.local @endPSet
.local @start
.local @checkYBounds
lda coX
cmp #MAX_X       ; if x > 159
bcc @checkYBounds         ; then @end
lda #$1
jmp @endPSet

@checkYBounds:
lda coY
cmp #MAX_Y        ; if y > 167
bcc @start         ; then @end
lda #$2
jmp @endPSet

@start:
DEBUG_PREPIXEL_DRAW
lda _picDrawEnabled
bne @drawPictureScreen         ; If picDrawEnabled == 0, skip to the end
jmp @endPSet

@drawPictureScreen:
SET_PICCOLOR

SET_VERA_ADDRESS_PICTURE coX, coY

lda _toDraw
sta VERA_data0

DEBUG_PIXEL_DRAW coX, coY

@endPSet:
DEBUG_PIXEL_DRAWN coX, coY
    ; Continue with the rest of the code

.endmacro

GO_NO_FURTHER_LEFT = ZP_TMP_13
GO_NO_FURTHER_RIGHT = ZP_TMP_14
_okToFillUpperCheckPoint: .word $0
_okToFillLowerCheckPoint_1: .word $0
_okToFillLowerCheckPoint_2: .word $0
_okToFillLowerCheckPoint_3: .word $0
_okToFillLowerCheckPoint_4: .word $0
_okToFillDebuggerCheckPoint: .word $0 ; Never used but needed to make the macro work

.macro PSET_ADDRESS address
.local @endPSet
.local @start
.local @checkYBounds
;DEBUG_PREPIXEL_DRAW

SET_VERA_ADDRESS_PICTURE_ADDRESS address, #$0

lda VERA_data0
cmp #$FF
beq @draw
@checkLeftBorder:
cmp #LEFT_BORDER
bne @checkRightBorder
lda #$1
sta GO_NO_FURTHER_LEFT
bra @draw
@checkRightBorder:
cmp #RIGHT_BORDER
bne @draw
lda #$1
sta GO_NO_FURTHER_RIGHT
@draw:
lda _toDraw
sta VERA_data0

DEBUG_PIXEL_DRAW_ADDRESS address
;DEBUG_PIXEL_DRAWN coX, coY
    ; Continue with the rest of the code

.endmacro


.macro GET_VERA_ADDRESS coX, coY, outputVar
.local @start
.local @originalZPTMP
bra @start
@originalZPTMP: .word $0

@start:
lda ZP_PREMULTIPLY_TABLE 
sta @originalZPTMP  ; Save ZP_TMP
lda ZP_PREMULTIPLY_TABLE +1
sta @originalZPTMP+1

lda coX
clc
adc #<STARTING_BYTE
sta outputVar
lda #$0
adc #>STARTING_BYTE
sta outputVar+1

clc
lda coY
adc ZP_PREMULTIPLY_TABLE ;  Loading from preMultiplyTable populated in C (picture.c). Add twice as each entry is two bytes
sta ZP_PREMULTIPLY_TABLE
lda #$0
adc ZP_PREMULTIPLY_TABLE+1 
sta ZP_PREMULTIPLY_TABLE+1 
lda coY
adc ZP_PREMULTIPLY_TABLE
sta ZP_PREMULTIPLY_TABLE
lda #$0
adc ZP_PREMULTIPLY_TABLE+1
sta ZP_PREMULTIPLY_TABLE+1 

clc
lda (ZP_PREMULTIPLY_TABLE)  ; y * BYTES_PER_ROW.
adc outputVar
sta outputVar
ldy #$1
lda (ZP_PREMULTIPLY_TABLE),y
adc outputVar+1
sta outputVar+1

lda @originalZPTMP
sta ZP_PREMULTIPLY_TABLE
lda @originalZPTMP+1
sta ZP_PREMULTIPLY_TABLE+1

.endmacro


.macro SET_VERA_ADDRESS_PICTURE coX, coY, stride
GET_VERA_ADDRESS coX, coY, _drawWhere

.ifnblank stride
lda stride << 4
.endif
.ifblank stride
lda #$10 ;High byte of address will always be 0
.endif

stz VERA_ctrl

sta VERA_addr_bank

lda _drawWhere + 1
sta VERA_addr_high

lda _drawWhere
sta VERA_addr_low

.endmacro

.macro SET_VERA_ADDRESS_PICTURE_ADDRESS address, stride
.ifnblank stride
lda stride << 4
.endif
.ifblank stride
lda #$10 ;High byte of address will always be 0
.endif

stz VERA_ctrl

sta VERA_addr_bank ;High byte of address will always be 0

lda address + 1
sta VERA_addr_high

lda address
sta VERA_addr_low

.endmacro

; void OK_TO_FILL_ADDRESS(int endLabel, int checkpoint) {
;     int A = _picDrawEnabled ^ 1;
;     int X = A;
;     A = _priDrawEnabled ^ 1;
;     temp = A;
;     A = X & temp;
;     if (A == 0) goto checkColorDefault;
;     goto returnZero;
    
;     checkColorDefault:
;     A = _picColour;
;     if (A != PIC_DEFAULT) goto checkPriDisable;
;     checkpoint = &colorDefaultCheckPoint;
;     goto returnZero;
    
;     checkPriDisable:
;     A = _priDrawEnabled;
;     if (A != 0) goto checkPriEnabledPicDisabled;
;     goto returnGetPixelResult;
    
;     checkPriEnabledPicDisabled:
;     A = _picDrawEnabled ^ 1;
;     A &= _priDrawEnabled;
;     if (A == 0) goto returnGetPixelResult;
;     goto returnZero;
    
;     returnGetPixelResult:
;     checkpoint = &returnGetPixelResultCheckPoint;
;     int okFillAddress = ...; // should be initialized before
;     A = PIC_GET_PIXEL_ADDRESS(okFillAddress);
;     if (A == PIC_DOUBLE_DEFAULT || A == RIGHT_BORDER || A == LEFT_BORDER) goto returnOne;
;     goto returnZero;
    
;     returnZero:
;     A = 0;
;     goto endLabel;
    
;     returnOne:
;     A = 1;
;     goto endLabel;
    
;     end:
; }
OK_TO_FILL_ADDRESS = ZP_TMP_12
.macro OK_TO_FILL_UPPER ; We need an upper and lower for the sake of the checkpoint jmp, which are two different addresses
.local endOkToFillUpper
.local returnZeroUpper
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroUpper
jmp (_okToFillUpperCheckPoint)
startOkToFillUpper:
OK_TO_FILL_ADDRESS endOkToFillUpper, _okToFillUpperCheckPoint
returnZeroUpper:
lda #$0
endOkToFillUpper:
.endmacro

.macro OK_TO_FILL_LOWER_1
.local returnZeroLower
.local endOkToFillLower
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroLower

jmp (_okToFillLowerCheckPoint_1)
startOkToFillLower_1:
OK_TO_FILL_ADDRESS endOkToFillLower, _okToFillLowerCheckPoint_1
returnZeroLower:
lda #$0
endOkToFillLower:
.endmacro

.macro OK_TO_FILL_LOWER_2
.local returnZeroLower
.local endOkToFillLower
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroLower

jmp (_okToFillLowerCheckPoint_2)
startOkToFillLower_2:
OK_TO_FILL_ADDRESS endOkToFillLower, _okToFillLowerCheckPoint_2
returnZeroLower:
lda #$0
endOkToFillLower:
.endmacro

.macro OK_TO_FILL_LOWER_3
.local returnZeroLower
.local endOkToFillLower
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroLower

jmp (_okToFillLowerCheckPoint_3)
startOkToFillLower_3:
OK_TO_FILL_ADDRESS endOkToFillLower, _okToFillLowerCheckPoint_3
returnZeroLower:
lda #$0
endOkToFillLower:
.endmacro

.macro OK_TO_FILL_LOWER_4
.local returnZeroLower
.local endOkToFillLower
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroLower

jmp (_okToFillLowerCheckPoint_4)
startOkToFillLower_4:
OK_TO_FILL_ADDRESS endOkToFillLower, _okToFillLowerCheckPoint_4
returnZeroLower:
lda #$0
endOkToFillLower:
.endmacro



.macro OK_TO_FILL_DEBUG
GREATER_THAN_OR_EQ_16_LITERAL OK_TO_FILL_ADDRESS, MAX_ADDRESS + 1, returnZeroDebugger
startOkToFillDebugger:
OK_TO_FILL_ADDRESS endOkToFillDebugger, _okToFillDebuggerCheckPoint
returnZeroDebugger:
lda #$0
endOkToFillDebugger:
.endmacro

.macro OK_TO_FILL_ADDRESS endLabel, checkpoint
.local @checkXBounds
.local @checkYBounds
.local @start
.local @priDisabledPicDisabled
.local @temp
.local @colorDefault
.local @checkColorDefault
.local @checkPriDisable
.local @checkPriEnabledPicDisabled
.local @returnGetPixelResult
.local @priDisablePicEnabled
.local @priEnabledPicEnabled
.local @priEnabledPicDisabled
.local @returnOne
.local @returnZero
.local @end

lda _picDrawEnabled
eor #$1
tax
lda _priDrawEnabled
eor #$1
sta @temp
txa
and @temp
beq @checkColorDefault
@priDisabledPicDisabled:
jmp @returnZero

@checkColorDefault:
lda _picColour
cmp #PIC_DEFAULT
bne @checkPriDisable

@colorDefault:
lda #<@colorDefaultCheckPoint
sta checkpoint
lda #>@colorDefaultCheckPoint
sta checkpoint + 1
@colorDefaultCheckPoint:
jmp @returnZero

@checkPriDisable:
lda _priDrawEnabled
bne @checkPriEnabledPicDisabled
@priDisablePicEnabled:
bra @returnGetPixelResult

@checkPriEnabledPicDisabled:
lda _picDrawEnabled
eor #$1
and _priDrawEnabled
@priEnabledPicEnabled:
beq @returnGetPixelResult

@priEnabledPicDisabled: ;ToDO
jmp @returnZero

@returnGetPixelResult:
lda #<@returnGetPixelResultCheckPoint
sta checkpoint
lda #>@returnGetPixelResultCheckPoint
sta checkpoint + 1
@returnGetPixelResultCheckPoint:
PIC_GET_PIXEL_ADDRESS OK_TO_FILL_ADDRESS
cmp #PIC_DOUBLE_DEFAULT ; Expected to have $FF as each pixel is double width. Just compare it with this value to save a shift
beq @returnOne
cmp #RIGHT_BORDER
beq @returnOne
cmp #LEFT_BORDER
beq @returnOne
@returnZero:
lda #$0
bra endLabel
@returnOne:
lda #$1
bra endLabel
@temp: .byte $0
@end:
.endmacro

ZP_PTR_FLOOD_QUEUE_STORE = ZP_TMP_21

; void FLOOD_Q_STORE(unsigned short* ZP_PTR_FLOOD_QUEUE_STORE) {
;     unsigned char q = 0;
;     unsigned short floodQueueEnd = 0;
;     unsigned short RAM_BANK = sposBank;

;     *ZP_PTR_FLOOD_QUEUE_STORE = q;
;     (*ZP_PTR_FLOOD_QUEUE_STORE)++; // Increment the queue pointer

;     if (*ZP_PTR_FLOOD_QUEUE_STORE == FLOOD_QUEUE_END) {
;         *ZP_PTR_FLOOD_QUEUE_STORE = FLOOD_QUEUE_START; // Reset the queue pointer to the start

;         if (RAM_BANK == LAST_FLOOD_BANK) {
;             RAM_BANK = FIRST_FLOOD_BANK;
;             sposBank = FIRST_FLOOD_BANK;
;         } else {
;             RAM_BANK++;
;             sposBank++;
;         }
;     }
; }
;Store position bank (sposBank) ZP_TMP_3
.macro FLOOD_Q_STORE
.local @floodQueueEnd
.local @start
.local @checkEnd
.local @incrementHighByte
.local @q
.local @end
.local @incBank
.local @end
;DEBUG_FLOOD_QUEUE_STORE @q
.segment "BANKRAMFLOOD"
@start:

ldx ZP_TMP_3
stx RAM_BANK

sta (ZP_PTR_FLOOD_QUEUE_STORE)

inc ZP_PTR_FLOOD_QUEUE_STORE
beq @incrementHighByte

@checkEnd:
NEQ_16_WORD_TO_LITERAL ZP_PTR_FLOOD_QUEUE_STORE, (FLOOD_QUEUE_END + 1), @end

lda #< FLOOD_QUEUE_START
sta ZP_PTR_FLOOD_QUEUE_STORE
lda #> FLOOD_QUEUE_START
sta ZP_PTR_FLOOD_QUEUE_STORE + 1

lda #LAST_FLOOD_BANK
cmp RAM_BANK
bne @incBank

lda #FIRST_FLOOD_BANK
sta RAM_BANK
sta ZP_TMP_3

bra @end

@incrementHighByte:
inc ZP_PTR_FLOOD_QUEUE_STORE + 1
bra @checkEnd

@incBank:
inc RAM_BANK ; The next flood bank will have identical code, so we can just increment the bank
inc ZP_TMP_3
@end:
.endmacro

ZP_PTR_FLOOD_QUEUE_RETRIEVE = ZP_TMP_22
;Retrieve position ZP_TMP_4 (rposBank)
.macro FLOOD_Q_RETRIEVE
.local @end
.local @serve
.local @resetBank
.local @returnResult
.local @serve
.local @incrementHighByte
.local @checkEnd
.local @returnEmpty


lda ZP_TMP_4
sta RAM_BANK

lda ZP_TMP_3
cmp ZP_TMP_4
bne @serve

lda ZP_PTR_FLOOD_QUEUE_STORE
cmp ZP_PTR_FLOOD_QUEUE_RETRIEVE
bne @serve

lda ZP_PTR_FLOOD_QUEUE_STORE + 1
cmp ZP_PTR_FLOOD_QUEUE_RETRIEVE + 1
bne @serve

jmp @returnEmpty
@serve:
lda (ZP_PTR_FLOOD_QUEUE_RETRIEVE)
tay

inc ZP_PTR_FLOOD_QUEUE_RETRIEVE
beq @incrementHighByte

@checkEnd:
NEQ_16_WORD_TO_LITERAL ZP_PTR_FLOOD_QUEUE_RETRIEVE, (FLOOD_QUEUE_END + 1), @returnResult

lda #< FLOOD_QUEUE_START
sta ZP_PTR_FLOOD_QUEUE_RETRIEVE
lda #> FLOOD_QUEUE_START
sta ZP_PTR_FLOOD_QUEUE_RETRIEVE + 1

lda ZP_TMP_4
cmp #LAST_FLOOD_BANK
beq @resetBank

inc RAM_BANK
inc ZP_TMP_4
bra @returnResult

@resetBank:
lda #FIRST_FLOOD_BANK
sta ZP_TMP_4
bra @returnResult
@returnEmpty:
ldx #QEMPTY
bra @end
@incrementHighByte:
inc ZP_PTR_FLOOD_QUEUE_RETRIEVE + 1
bra @checkEnd

@returnResult:
tya
DEBUG_FLOOD_QUEUE_RETRIEVE 
ldx #$0
@end:
.endmacro

.macro DRAW_LINE_BETWEEN p1, p2 ;Assumes that the VERA address is already set to vera port 1
.local @loop
.local @checkLoop
.local @end

lda p2
ldx _toDraw

@loop:
stx VERA_data0

@checkLoop:
cmp p1
beq @end
dec
bra @loop
@end:
.endmacro


ZP_DIV_AREA = ZP_TMP_2
ZP_DIV_BANK = ZP_TMP_23
ZP_DIV_ADDR = ZP_TMP_24 
_floatDivision:
bra @start
@numerator: .word $0 ; Even though numerator is only one byte we double it for address looked up
@denominator: .word $0 ; Even though denominator is only one byte we double it for address looked up
@originalZPCh: .word $0 ;For Division Bank Table
@originalZPDisp: .word $0 ;For Division Address Table
@previousRamBank: .byte $0
@start:
dec
dec
sta @denominator

jsr popa
dec
sta @numerator

lda RAM_BANK
sta @previousRamBank

lda ZP_DIV_BANK
sta @originalZPCh
lda ZP_DIV_BANK+1
sta @originalZPCh+1

lda ZP_DIV_ADDR
sta @originalZPDisp
lda ZP_DIV_ADDR+1
sta @originalZPDisp+1

lda @numerator
clc
adc ZP_DIV_BANK
sta ZP_DIV_BANK
lda #$0
adc ZP_DIV_BANK + 1
sta ZP_DIV_BANK + 1

lda #DIVISION_METADATA_BANK
sta RAM_BANK

lda (ZP_DIV_BANK)
tax

lda @originalZPCh
sta ZP_DIV_BANK

lda @originalZPCh + 1
sta ZP_DIV_BANK + 1


lda @numerator
clc
asl 
sta @numerator
lda #$0 ; always zero
rol
sta @numerator+1

lda @numerator
clc
adc ZP_DIV_ADDR
sta ZP_DIV_ADDR
lda @numerator+1
adc ZP_DIV_ADDR + 1
sta ZP_DIV_ADDR + 1

lda (ZP_DIV_ADDR)
sta ZP_DIV_AREA
ldy #$1
lda (ZP_DIV_ADDR),y
sta ZP_DIV_AREA+1

lda @originalZPDisp
sta ZP_DIV_ADDR
lda @originalZPDisp + 1
sta ZP_DIV_ADDR + 1

lda @denominator
pha
clc
asl 
sta @denominator
lda #$0 ; always zero
rol
sta @denominator+1

pla
clc
adc @denominator
sta @denominator
lda #$0
adc @denominator+1
sta @denominator+1

lda @denominator
clc
adc ZP_DIV_AREA
sta ZP_DIV_AREA
lda @denominator+1
adc ZP_DIV_AREA+1
sta ZP_DIV_AREA+1

stx RAM_BANK
stz sreg + 1
ldy #$1
lda (ZP_DIV_AREA),y
tax
ldy #$2
lda (ZP_DIV_AREA),y
sta sreg
lda (ZP_DIV_AREA)

ldy @previousRamBank
sty RAM_BANK

rts

.segment "BANKRAM05"
_b5DebugPixelDrawAddressAsm:
sta _logDebugVal1
stx _logDebugVal2
PRINT_PIXEL_MESSAGE _b5DebugPixelDrawAddress
rts


_b5DebugPixelDrawAsm:
sta _logDebugVal1
stx _logDebugVal2
PRINT_PIXEL_MESSAGE _b5DebugPixelDraw
rts

_b5DebugPrePixelDrawAsm:
clc
lda #$1
adc _pixelCounter
sta _pixelCounter
lda #$0
adc _pixelCounter + 1
sta _pixelCounter + 1
lda #$0
adc _pixelCounter + 2
sta _pixelCounter + 2
lda #$0
adc _pixelCounter + 3
sta _pixelCounter + 3

EQ_32_LONG_TO_LITERAL _pixelStopAt, NEG_1_16, NEG_1_16, @checkFreeze
LESS_THAN_32 _pixelCounter, _pixelStopAt, @checkFreeze, @stop
@stop:
nop ;There to make it clearer where we have stopped
@checkFreeze:

EQ_32_LONG_TO_LITERAL _pixelFreezeAt, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelFreezeAt, @end, @freeze
@freeze:
lda var1 ;Pointless instruction to make it clearer what the points are on freeze
lda var2
bra @freeze
@end:
rts

_b5DebugPixelDrawnAsm:
sta _logDebugVal1
stx _logDebugVal2
PRINT_PIXEL_MESSAGE _b5CheckPixelDrawn
rts

.segment "BANKRAMFLOOD"
.ifdef TEST_IS_MULTIPLE_OF_160
_testIsMultipleOf160Asm:
sta @toTest
stx @toTest + 1
IS_MULT_OF_160 @toTest
rts
@toTest: .word $0

.export _testIsMultipleOf160Asm
.endif


PIC_DOUBLE_DEFAULT = PIC_DEFAULT * $10 + PIC_DEFAULT
PIC_DEFAULT = $F
PRI_DEFAULT = 4
.macro PIC_GET_PIXEL coX, coY
.local @end
.local @returnDefault

lda coX
cmp #MAX_X
bcs @returnDefault

lda coY
cmp #MAX_Y
bcs @returnDefault

SET_VERA_ADDRESS_PICTURE coX, coY
lda VERA_data0
lsr
lsr
lsr
lsr
bra @end
@returnDefault:
lda #PIC_DEFAULT
@end:
ldx #$0
.endmacro

FLOOD_QUEUE_START = $A850
FLOOD_QUEUE_END = $BEAF

_bFloodPicGetPixel:
bra @start
@coX: .byte $0
@coY: .byte $0
@start:
sta @coY
jsr popax
sta @coX

PIC_GET_PIXEL @coX, @coY
rts

.macro PIC_GET_PIXEL_ADDRESS address
.local @end
.local @returnDefault
.local @start

lda #$10
sta VERA_addr_bank ; Stride 1. High byte of address will always be 0
lda address + 1
sta VERA_addr_high

lda address
sta VERA_addr_low

lda VERA_data0
.endmacro

_bFloodOkToFill:
sta OK_TO_FILL_ADDRESS
stx OK_TO_FILL_ADDRESS + 1
OK_TO_FILL_DEBUG
rts
@temp: .byte $0

; /**************************************************************************
; ** agiFill
; **************************************************************************/
; // Function to perform flood fill
; void bFloodAgiFill(byte fillX, byte fillY) {
;     int _okFillAddress = 0, storeCounter = 0, loopCounter = 0;
;     int* okToFillUpperCheckPoint = &startOkToFillUpper, okToFillLowerCheckPoint = &startOkToFillLower;
;     bool _goNoFurtherLeft = 0, _goNoFurtherRight = 0;
;     bool ok = false;

;     // Set initial values
;     SET_PICCOLOR();
;     GET_VERA_ADDRESS(fillX, fillY, &_okFillAddress);

;     // initialStore loop
;     _okFillAddress = _bFloodQstore();

;     // Fill Loop
;     while (true) {
;         // Reset flags
;         _goNoFurtherLeft = 0;
;         _goNoFurtherRight = 0;
;         loopCounter = 0;
;         
;    // RetrieveLoop. A loop in assembly as you need to get x and y        
;         bool isEmpty; 
;         _okFillAddress = FLOOD_Q_RETRIEVE(&isEmpty);
;         if (isEmpty) {
;             return;
;         }
        
;         // checkXYOKFill
;         ok = OK_TO_FILL_ADDRESS(_okFillAddress);
;         if (!ok) {
;             continue;
;         }
;         // pSet
;         bool goNoFurtherLeft, goNoFurtherRight
;         PSET_ADDRESS(_okFillAddress, &goNoFurtherLeft, &goNoFurtherRight);

;         // storeFillChecks
;         if (OK_TO_FILL_ADDRESS(_okFillAddress - NO_BYTES_PER_LINE)) {
;             _bFloodQstore(_okFillAddress - NO_BYTES_PER_LINE);
;         }
;         if (OK_TO_FILL_ADDRESS(_okFillAddress - 1) && !goNoFurtherLeft) {
;             _bFloodQstore(_okFillAddress - 1);
;         }
;         if (OK_TO_FILL_ADDRESS(_okFillAddress + 1) && !goNoFurtherRight) {
;             _bFloodQstore(_okFillAddress + 1);
;         }
;         if (OK_TO_FILL_ADDRESS(_okFillAddress + NO_BYTES_PER_LINE)) {
;             _bFloodQstore(_okFillAddress + NO_BYTES_PER_LINE);
;         }
;     }
; }
;This won't fit on the picture bank
.segment "BANKRAM02"
_b2DrawStraightLineAlongY: 
nop
nop
nop
sta ZP_TMP_5 ;x The C which calls this function may invert it's own x1 and x2 depending on which is larger
jsr popax
sta ZP_TMP_7 ;y2
stx ZP_TMP_6 ;y1

ldy _picDrawEnabled
beq @end

SET_VERA_ADDRESS_PICTURE ZP_TMP_5 , ZP_TMP_6, #$D

DRAW_LINE_BETWEEN ZP_TMP_6, ZP_TMP_7

@end:
rts

_b2DrawStraightLineAlongX:
sta ZP_TMP_5 ;y1 The C which calls this function may invert it's own y1 and y2 depending on which is larger
jsr popax
sta ZP_TMP_7 ;x2
stx ZP_TMP_6 ;x1

ldy _picDrawEnabled
beq @end

SET_VERA_ADDRESS_PICTURE ZP_TMP_6 , ZP_TMP_5, #$1

DRAW_LINE_BETWEEN ZP_TMP_6, ZP_TMP_7

@end:
rts


.segment "BANKRAM11"
initFlood:

sta fillY
jsr popa 
sta fillX

lda #< startOkToFillUpper
sta _okToFillUpperCheckPoint
lda #> startOkToFillUpper
sta _okToFillUpperCheckPoint + 1

lda #< startOkToFillLower_1
sta _okToFillLowerCheckPoint_1
lda #> startOkToFillLower_1
sta _okToFillLowerCheckPoint_1 + 1

lda #< startOkToFillLower_2
sta _okToFillLowerCheckPoint_2
lda #> startOkToFillLower_2
sta _okToFillLowerCheckPoint_2 + 1

lda #< startOkToFillLower_3
sta _okToFillLowerCheckPoint_3
lda #> startOkToFillLower_3
sta _okToFillLowerCheckPoint_3 + 1

lda #< startOkToFillLower_4
sta _okToFillLowerCheckPoint_4
lda #> startOkToFillLower_4
sta _okToFillLowerCheckPoint_4 + 1


;Set initial values
SET_PICCOLOR
GET_VERA_ADDRESS fillX, fillY, OK_TO_FILL_ADDRESS
rts

.segment "BANKRAMFLOOD"

TO_STORE = ZP_TMP_5 ; 8 bytes All the way to ZP_TMP_8
LOOP_COUNTER = ZP_TMP_9
STORE_COUNTER = ZP_TMP_10
_bFloodAgiFill:

tax
lda #PICTURE_BANK
sta tmp4

lda #< initFlood
sta ptr4
lda #> initFlood
sta ptr4 + 1

txa
jsr _trampoline

;InitialStore loop
ldy #$0
initialStore:
lda OK_TO_FILL_ADDRESS,y
sty STORE_COUNTER
jsr _bFloodQstore
ldy STORE_COUNTER
iny
cpy #$2
bcs fillLoop
jmp initialStore

;Fill loop
fillLoop:

;Reset flags
stz GO_NO_FURTHER_LEFT
stz GO_NO_FURTHER_RIGHT

FLOOD_Q_RETRIEVE
ldy #$0
sta OK_TO_FILL_ADDRESS,y
cpx #QEMPTY
beq @goToEnd

FLOOD_Q_RETRIEVE
ldy #$1
sta OK_TO_FILL_ADDRESS,y
cpx #QEMPTY
bne checkXYOKFill

@goToEnd:
jmp fillEnd

checkXYOKFill:
OK_TO_FILL_UPPER
bne @pSet
jmp fillLoop

@pSet:
PSET_ADDRESS OK_TO_FILL_ADDRESS

storeFillChecks:
sec
lda OK_TO_FILL_ADDRESS
sbc #< BYTES_PER_ROW
sta TO_STORE

lda OK_TO_FILL_ADDRESS + 1
sbc #> BYTES_PER_ROW
sta TO_STORE + 1
 
sec
lda OK_TO_FILL_ADDRESS
sbc #$1
sta TO_STORE + 2

lda OK_TO_FILL_ADDRESS + 1
sbc #$0
sta TO_STORE + 3

clc
lda OK_TO_FILL_ADDRESS
adc #$1
sta TO_STORE + 4

lda OK_TO_FILL_ADDRESS + 1
adc #$0
sta TO_STORE + 5

clc
lda OK_TO_FILL_ADDRESS
adc #< BYTES_PER_ROW
sta TO_STORE + 6

lda OK_TO_FILL_ADDRESS + 1
adc #> BYTES_PER_ROW
sta TO_STORE + 7

ldy #$0
NEIGHBOURHOODCHECK OK_TO_FILL_LOWER_1
ldy #$2
NEIGHBOURHOODCHECK OK_TO_FILL_LOWER_2
ldy #$4
NEIGHBOURHOODCHECK OK_TO_FILL_LOWER_3
ldy #$6
NEIGHBOURHOODCHECK OK_TO_FILL_LOWER_4
jmp fillLoop
fillEnd:

rts
.segment "CODE"
fillX: .byte $0
fillY: .byte $0
.segment "BANKRAMFLOOD"

_bFloodQstore:
FLOOD_Q_STORE
rts

QEMPTY = $FF

; void FLOOD_Q_RETRIEVE() {
;     unsigned short RAM_BANK = rposBank;
;     unsigned char X; // Using 'X' to represent 6502's X register

;     if (*ZP_PTR_FLOOD_QUEUE_RETRIEVE == FLOOD_QUEUE_END + 1) {
;         *ZP_PTR_FLOOD_QUEUE_RETRIEVE = FLOOD_QUEUE_START;

;         rposBank++;
;         if (rposBank == LAST_FLOOD_BANK + 1) {
;             rposBank = FIRST_FLOOD_BANK;
;             return QEMPTY; // Assuming QEMPTY is a status indicating the queue is empty
;         }
;     }

;     X = *ZP_PTR_FLOOD_QUEUE_RETRIEVE;
;     (*ZP_PTR_FLOOD_QUEUE_RETRIEVE)++;

;     return X; // Assuming this function returns the value from the queue
; }
.endif