; Check if global definitions are included, if not, include them
.ifndef  PICTURE_INC
PICTURE_INC = 1

.include "x16.inc"
.include "global.s"
.include "graphics.s"


.import _picColour
.import _picDrawEnabled
.import _priDrawEnabled
.import _noSound

.import _rpos
.import _spos
.import _rposBank
.import _sposBank

.import _trampoline_0
.import _b11FloodBankFull

.import popa
.import popax
.import pushax

.import picDrawEnabled
.import priDrawEnabled

.segment "CODE"

DEBUG_PIXEL_DRAW = 1


.import _b5DebugPixelDraw
.import _b5DebugPrePixelDraw
.import _b5CheckPixelDrawn
.import _stopAtPixel
.import _b5DebugFloodQueueRetrieve
.import _b5DebugFloodQueueStore
.import _pixelCounter
.import _pixelStartPrintingAt
.import _pixelStopAt
.import _queueAction
.import _pixelFreezeAt


.ifdef DEBUG_CHECK_LINE_DRAWN
.import _b5LineDrawDebug
.endif

MAX_X = 160
MAX_Y = 168

.macro PRINT_PIXEL_MESSAGE printFunc
.local @end
EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelStartPrintingAt, @end
JSRFAR printFunc, DEBUG_BANK
@end:
.endmacro

.macro DEBUG_PIXEL_DRAW var1, var2
.ifdef DEBUG_PIXEL_DRAW
lda var1
ldx var2
JSRFAR _b5DebugPixelDrawAsm, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_PREPIXEL_DRAW var1, var2
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

.macro DEBUG_FLOOD_QUEUE_STORE var1, var2
.local @end
.ifdef DEBUG_PIXEL_DRAW
EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelStartPrintingAt, @end
lda var1
sta _logDebugVal1
JSRFAR _b5DebugFloodQueueStore, DEBUG_BANK
@end:
INC_32 _queueAction
.endif
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
DEBUG_PREPIXEL_DRAW coX, coY
lda _picDrawEnabled
bne @drawPictureScreen         ; If picDrawEnabled == 0, skip to the end
jmp @endPSet

@drawPictureScreen:
lda _picColour
asl a           ; Shift left 4 times to multiply by 16
asl a  
asl a  
asl a  
ora _picColour
sta _toDraw     ; toDraw = picColour << 4 | picColour

SET_VERA_ADDRESS coX, coY

lda _toDraw
sta VERA_data0

DEBUG_PIXEL_DRAW coX, coY

@endPSet:
DEBUG_PIXEL_DRAWN coX, coY
    ; Continue with the rest of the code

.endmacro

.macro SET_VERA_ADDRESS coX, coY
.local @start
.local @originalZPTMP
bra @start
@originalZPTMP: .word $0

@start:
lda ZP_TMP 
sta @originalZPTMP  ; Save ZP_TMP
lda ZP_TMP+1
sta @originalZPTMP+1

lda coX
clc
adc #<STARTING_BYTE
sta _drawWhere
lda #$0
adc #>STARTING_BYTE
sta _drawWhere+1

clc
lda coY
adc ZP_TMP ;  Loading from preMultiplyTable populated in C (picture.c). Add twice as each entry is two bytes
sta ZP_TMP
lda #$0
adc ZP_TMP+1 
sta ZP_TMP+1 
lda coY
adc ZP_TMP
sta ZP_TMP
lda #$0
adc ZP_TMP+1
sta ZP_TMP+1 

clc
lda (ZP_TMP)  ; y * BYTES_PER_ROW.
adc _drawWhere
sta _drawWhere
ldy #$1
lda (ZP_TMP),y
adc _drawWhere+1
sta _drawWhere+1

lda @originalZPTMP
sta ZP_TMP
lda @originalZPTMP+1
sta ZP_TMP+1

lda #$10
sta VERA_addr_bank ; Stride 1. High byte of address will always be 0


lda _drawWhere + 1
sta VERA_addr_high

lda _drawWhere
sta VERA_addr_low

.endmacro

;boolean bFloodOkToFill(byte x, byte y)
;{
;	boolean getPicResult;
;   if(x > 160 || y > 168) return FALSE;
;	
;   if (!picDrawEnabled && !priDrawEnabled) return FALSE;
;	if (picColour == PIC_DEFAULT) return FALSE;
;	if (!priDrawEnabled)
;	{
;		getPicResult = bFloodPicGetPixel(x, y);
;		return (getPicResult == PIC_DEFAULT);
;	}
;	if (priDrawEnabled && !picDrawEnabled) return (bFloodPriGetPixel(x, y) == PRI_DEFAULT);
;	return (bFloodPicGetPixel(x, y) == PIC_DEFAULT);
;}

.macro OK_TO_FILL
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
.local @returnZero
.local @end

@checkXBounds:
cmp #MAX_X       ; if x > 159
bcc @checkYBounds         ; then @returnZero
lda #$1
jmp @returnZero

@checkYBounds:
sta _okFillX
cpx #MAX_Y        ; if y > 167
bcc @start         ; then @returnZero
lda #$2
jmp @returnZero

@start:
stx _okFillY
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
PIC_GET_PIXEL _okFillX, _okFillY
cmp #PIC_DEFAULT
bne @returnZero
lda #$1
bra @end
@returnZero:
lda #$0
bra @end
@temp: .byte $0
@end:
.endmacro

; void FLOOD_Q_STORE(unsigned short* ZP_PTR_B1) {
;     unsigned char q = 0;
;     unsigned short floodQueueEnd = 0;
;     unsigned short RAM_BANK = _sposBank;

;     *ZP_PTR_B1 = q;
;     (*ZP_PTR_B1)++; // Increment the queue pointer

;     if (*ZP_PTR_B1 == FLOOD_QUEUE_END) {
;         *ZP_PTR_B1 = FLOOD_QUEUE_START; // Reset the queue pointer to the start

;         if (RAM_BANK == LAST_FLOOD_BANK) {
;             RAM_BANK = FIRST_FLOOD_BANK;
;             _sposBank = FIRST_FLOOD_BANK;
;         } else {
;             RAM_BANK++;
;             _sposBank++;
;         }
;     }
; }
.macro FLOOD_Q_STORE
.local @floodQueueEnd
.local @start
.local @q
.local @end
.local @incBank
.local @end
sta _logDebugVal1
sta @q
DEBUG_FLOOD_QUEUE_STORE @q
bra @start
.segment "CODE"
@q: .byte $0
.segment "BANKRAMFLOOD"
@floodQueueEnd: .word $0
@start:
lda _sposBank
sta RAM_BANK

lda @q
sta (ZP_PTR_B1)

clc
lda #$1
adc ZP_PTR_B1
sta ZP_PTR_B1

lda #$0
adc ZP_PTR_B1 + 1
sta ZP_PTR_B1 + 1
NEQ_16_WORD_TO_LITERAL ZP_PTR_B1, (FLOOD_QUEUE_END + 1), @end

lda #< FLOOD_QUEUE_START
sta ZP_PTR_B1
lda #> FLOOD_QUEUE_START
sta ZP_PTR_B1 + 1

lda #LAST_FLOOD_BANK
cmp RAM_BANK
bne @incBank

lda #FIRST_FLOOD_BANK
sta RAM_BANK
sta _sposBank

ldx #> _b11FloodBankFull
lda #< _b11FloodBankFull
jsr pushax
lda #PICTURE_BANK
ldx #$0
jsr _trampoline_0

bra @end

@incBank:
inc RAM_BANK ; The next flood bank will have identical code, so we can just increment the bank
inc _sposBank
@end:
.endmacro

.macro FLOOD_Q_RETRIEVE
.local @end
.local @serve
.local @resetBank
.local @returnResult
.local @serve
.local @returnEmpty

lda _rposBank
sta RAM_BANK

lda _sposBank
cmp _rposBank
bne @serve

lda ZP_PTR_B1
cmp ZP_PTR_B2
bne @serve

lda ZP_PTR_B1 + 1
cmp ZP_PTR_B2 + 1
beq @returnEmpty

@serve:
lda (ZP_PTR_B2)
tay

clc
lda #$1
adc ZP_PTR_B2
sta ZP_PTR_B2

lda #$0
adc ZP_PTR_B2 + 1
sta ZP_PTR_B2 + 1

NEQ_16_WORD_TO_LITERAL ZP_PTR_B2, (FLOOD_QUEUE_END + 1), @returnResult

lda #< FLOOD_QUEUE_START
sta ZP_PTR_B2
lda #> FLOOD_QUEUE_START
sta ZP_PTR_B2 + 1

lda _rposBank
cmp #LAST_FLOOD_BANK
beq @resetBank

inc RAM_BANK
inc _rposBank
bra @returnResult

@resetBank:
lda #FIRST_FLOOD_BANK
sta _rposBank
bra @returnResult

@returnEmpty:
lda #QEMPTY
jmp @end

@returnResult:
tya
DEBUG_FLOOD_QUEUE_RETRIEVE 
@end:
ldx #$0
.endmacro


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

lda ZP_PTR_CH
sta @originalZPCh
lda ZP_PTR_CH+1
sta @originalZPCh+1

lda ZP_PTR_DISP
sta @originalZPDisp
lda ZP_PTR_DISP+1
sta @originalZPDisp+1

lda @numerator
clc
adc ZP_PTR_CH
sta ZP_PTR_CH
lda #$0
adc ZP_PTR_CH + 1
sta ZP_PTR_CH + 1

lda #DIVISION_METADATA_BANK
sta RAM_BANK

lda (ZP_PTR_CH)
tax

lda @originalZPCh
sta ZP_PTR_CH

lda @originalZPCh + 1
sta ZP_PTR_CH + 1


lda @numerator
clc
asl 
sta @numerator
lda #$0 ; always zero
rol
sta @numerator+1

lda @numerator
clc
adc ZP_PTR_DISP
sta ZP_PTR_DISP
lda @numerator+1
adc ZP_PTR_DISP + 1
sta ZP_PTR_DISP + 1

lda (ZP_PTR_DISP)
sta ZP_TMP_2
ldy #$1
lda (ZP_PTR_DISP),y
sta ZP_TMP_2+1

lda @originalZPDisp
sta ZP_PTR_DISP
lda @originalZPDisp + 1
sta ZP_PTR_DISP + 1

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
adc ZP_TMP_2
sta ZP_TMP_2
lda @denominator+1
adc ZP_TMP_2+1
sta ZP_TMP_2+1

stx RAM_BANK
stz sreg + 1
ldy #$1
lda (ZP_TMP_2),y
tax
ldy #$2
lda (ZP_TMP_2),y
sta sreg
lda (ZP_TMP_2)

ldy @previousRamBank
sty RAM_BANK

rts

.segment "BANKRAM05"
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
stp
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

.segment "BANKRAM11"
_b11PSet:
sta @coY
jsr popa
sta @coX
PSET @coX, @coY
rts
@coX: .byte $0
@coY: .byte $0

.segment "BANKRAMFLOOD"

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

SET_VERA_ADDRESS coX, coY
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

_bFloodOkToFill:
lda _okFillX
ldx _okFillY
OK_TO_FILL
rts
@temp: .byte $0

; /**************************************************************************
; ** agiFill
; **************************************************************************/
; void bFloodAgiFill(word x, word y)
; {
; 	byte x1, y1;
; #ifdef TEST_QUEUE
; 	testQueue();
; #endif // TEST_QUEUE

; #ifdef VERBOSE_FLOOD_FILL
; 	if (pixelCounter >= pixelStartPrintingAt && pixelStartPrintingAt != 1) {
; 		printf("bl\n");
; 	}
; #endif

; 	bFloodQstore(x);
; 	bFloodQstore(y);

; 	for (;;) {
;
; 		      x1 = qretrieve();
;             y1 = qretrieve();

; 		if ((x1 == QEMPTY) || (y1 == QEMPTY))
; 			break;
; 		else {

; 			okFillX = x1;
; 			okFillY = y1;
; 			if (bFloodOkToFill()) {

; 				PSETFLOOD(x1, y1);

; 				okFillX = x1;
; 				okFillY = y1 -1;
; 				if (bFloodOkToFill() && (y1 != 0)) {
; #ifdef VERBOSE_FLOOD_FILL
; 					if (pixelCounter >= pixelStartPrintingAt && pixelStartPrintingAt != 1) {
; 						printf("1\n");
; 					}
; #endif
; 					bFloodQstore(x1);
; 					bFloodQstore(y1 - 1);
; 				}
; 				okFillX = x1 - 1;
; 				okFillY = y1;
; 				if (bFloodOkToFill() && (x1 != 0)) {
; #ifdef VERBOSE_FLOOD_FILL
; 					if (pixelCounter >= pixelStartPrintingAt && pixelStartPrintingAt != 1) {
; 						printf("2\n");
; 					}
; #endif
; 					bFloodQstore(x1 - 1);
; 					bFloodQstore(y1);
; 				}

; 				okFillX = x1 + 1;
; 				okFillY = y1;
; 				if (bFloodOkToFill() && (x1 != 159)) {
; #ifdef VERBOSE_FLOOD_FILL
; 					if (pixelCounter >= pixelStartPrintingAt && pixelStartPrintingAt != 1) {
; 						printf("3\n");
; 					}
; #endif
; 					bFloodQstore(x1 + 1);
; 					bFloodQstore(y1);
; 				}

; 				okFillX = x1;
; 				okFillY = y1 + 1;
; 				if (bFloodOkToFill() && (y1 != 167)) {
; #ifdef VERBOSE_FLOOD_FILL
; 					if (pixelCounter >= pixelStartPrintingAt && pixelStartPrintingAt != 1) {
; 						printf("4\n");
; 					}
; #endif
; 					bFloodQstore(x1);
; 					bFloodQstore(y1 + 1);
; 				}

; 			}

; 		}

; 	}

; }
_bFloodAgiFill:
sta @y
jsr popa 
sta @x

ldy #$0
@initialStore:
lda @x,y
sty @storeCounter
jsr _bFloodQstore
ldy @storeCounter
iny
cpy #$2
bcs @fillLoop
jmp @initialStore

@fillLoop:
lda #$0
sta @loopCounter
@retrieveLoop:
FLOOD_Q_RETRIEVE
ldy @loopCounter
sta @x,y
cmp #QEMPTY
bne @checkIfRetrieveLoopShouldContinue
jmp @end
@checkIfRetrieveLoopShouldContinue:
inc @loopCounter
ldy @loopCounter
cpy #$2
bcs @checkXYOKFill
jmp @retrieveLoop

@checkXYOKFill:
lda @x
ldx @y
OK_TO_FILL
bne @isOkToFill
jmp @fillLoop
@isOkToFill:
PSET @x, @y

@storeFillChecks:
lda @x 
sta @toStore
lda @y
dec
sta @toStore + 1

lda @x
dec
sta @toStore + 2
lda @y
sta @toStore + 3

lda @x
inc
sta @toStore + 4
lda @y
sta @toStore + 5

lda @x
sta @toStore + 6
lda @y
inc
sta @toStore + 7

ldy #$0
sty @loopCounter
@neighbourCheckLoop:
ldy @loopCounter

lda @toStore,y
sta @x
iny
ldx @toStore,y
stx @y
OK_TO_FILL
bne @storeInQueue
jmp @checkNeighbourHoodLoopCounter
@storeInQueue:
ldy #$0
sty @storeCounter

@storeLoop:
lda @x,y
FLOOD_Q_STORE

inc @storeCounter
ldy @storeCounter
cpy #$2
bcs @checkNeighbourHoodLoopCounter
jmp @storeLoop
@checkNeighbourHoodLoopCounter:

inc @loopCounter
inc @loopCounter
ldy @loopCounter
cpy #8
bne @jmpBackToNeighbourCheckLoop
jmp @fillLoop
@jmpBackToNeighbourCheckLoop:
jmp @neighbourCheckLoop
@end:

rts
.segment "CODE"
@x: .byte $0
@y: .byte $0
@toStore: .res 8
@loopCounter: .byte $0
@storeCounter: .byte $0
_okFillX: .byte $0
_okFillY: .byte $0
.segment "BANKRAMFLOOD"

_bFloodQstore:
FLOOD_Q_STORE
rts

QEMPTY = $FF

; void FLOOD_Q_RETRIEVE() {
;     unsigned short RAM_BANK = _rposBank;
;     unsigned char X; // Using 'X' to represent 6502's X register

;     if (*ZP_PTR_B2 == FLOOD_QUEUE_END + 1) {
;         *ZP_PTR_B2 = FLOOD_QUEUE_START;

;         _rposBank++;
;         if (_rposBank == LAST_FLOOD_BANK + 1) {
;             _rposBank = FIRST_FLOOD_BANK;
;             return QEMPTY; // Assuming QEMPTY is a status indicating the queue is empty
;         }
;     }

;     X = *ZP_PTR_B2;
;     (*ZP_PTR_B2)++;

;     return X; // Assuming this function returns the value from the queue
; }
.endif