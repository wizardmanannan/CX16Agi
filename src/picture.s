; Check if global definitions are included, if not, include them
.ifndef  PICTURE_INC
PICTURE_INC = 1

.include "x16.inc"
.include "global.s"
.include "graphics.s"

.import _picColour
.import _picDrawEnabled
.import _noSound

.import _rpos
.import _spos
.import _rposBank
.import _sposBank

.import popa
.import popax

.segment "CODE"

DEBUG_PIXEL_DRAW = 1

.ifdef DEBUG_PIXEL_DRAW
.import _b5DebugPixelDraw
.import _b5DebugPrePixelDraw
.import _b5CheckPixelDrawn
.endif

.ifdef DEBUG_PIXEL_DRAW
.import _b5DebugFloodQueueRetrieve
.import _b5DebugFloodQueueStore
.import _pixelCounter
.import _pixelStartPrintingAt
.import _pixelStopAt
.import _queueAction
.endif

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
sta _logDebugVal1
lda var2
sta _logDebugVal2
PRINT_PIXEL_MESSAGE _b5DebugPixelDraw
.endif
.endmacro

.macro DEBUG_PREPIXEL_DRAW var1, var2
.local @stop
.local @end


.ifdef DEBUG_PIXEL_DRAW
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

PRINT_PIXEL_MESSAGE _b5DebugPrePixelDraw

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

EQ_32_LONG_TO_LITERAL _pixelCounter, NEG_1_16, NEG_1_16, @end
LESS_THAN_32 _pixelCounter, _pixelStopAt, @end, @stop
@stop:
stp
nop ;There to make it clearer where we have stopped
@end:
.endif
.endmacro

.macro DEBUG_PIXEL_DRAWN var1, var2
.ifdef DEBUG_PIXEL_DRAW
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
PRINT_PIXEL_MESSAGE _b5CheckPixelDrawn
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

.segment "BANKRAM11"
	;*****************************************************************
		; negate accumulator
		;*****************************************************************

neg:		eor #$ff
		clc
		adc #1
		rts

		;*****************************************************************
		; init bresenham line 
		;*****************************************************************
bresenham_sx: .byte $0 
bresenham_err: .word $0
bresenham_x1: .byte $0
bresenham_x2: .byte $0
bresenham_y1: .byte $0
bresenham_y2: .byte $0
bresenham_dx: .word $0
bresenham_sy: .byte $0
bresenham_dy: .word $0

b11Init_Bresenham:
		; dx = abs(x2 - x1)
		; dy = abs(y2 - y1)
		; sx = x1 < x2 ? 1 : -1
		; sy = y1 < y2 ? 1 : -1
		; err = dx > dy ? dx : -dy
		; dx = dx * 2
		; dy = dy * 2

		; if y1 < y2:
		; 	sy = 1
		; 	dy = y2 - y1
		; else:
		; 	sy = -1
		; 	dy = y1 - y2
		ldx #$ff		; X = -1
		lda bresenham_y1
		sec
		sbc bresenham_y2	; A = y1 - y2
		bpl :+
		ldx #1			; X = 1
		jsr neg			; A = y2 - y1
:		sta bresenham_dy
		stx bresenham_sy

		; if x1 < x2:
		; 	sx = 1
		; 	dx = x2 - x1
		; else:
		; 	sx = -1
		; 	dx = x1 - x2
		ldx #$ff		; X = -1
		lda bresenham_x1
		sec
		sbc bresenham_x2	; A = x1 - x2
		bpl :+
		ldx #1			; X = 1
		jsr neg			; A = x2 - x1
:		sta bresenham_dx
		stx bresenham_sx

		; err = dx > dy ? dx : -dy
		;lda bresenham_dx
		cmp bresenham_dy	; dx - dy > 0
		beq :+
		bpl @skiperr
:		lda bresenham_dy
		jsr neg
@skiperr:	sta bresenham_err

		; dx = dx * 2
		; dy = dy * 2
		asl bresenham_dx
		asl bresenham_dy
		rts

		;*****************************************************************
		; step along bresenham line
		;*****************************************************************

_b11Drawline:
		sta bresenham_y2
		jsr popax
		sta bresenham_x2
		stx bresenham_y1
		jsr popa
		sta bresenham_x1


		DEBUG_LINE_DRAW bresenham_x1, bresenham_y1, bresenham_x2, bresenham_y2
		DEBUG_PREPIXEL_DRAW bresenham_x1, bresenham_y1
        PSET bresenham_x1, bresenham_y1
		DEBUG_PIXEL_DRAWN bresenham_x1, bresenham_y1
		jsr b11Init_Bresenham

		; err2 = err

        drawLineStart:
		lda bresenham_err
		pha			; push err2

		; if err2 > -dx:
		;   err = err - dy
		;   x = x + sx
		clc
		adc bresenham_dx	; skip if err2 + dx <= 0
		bmi :+
		beq :+
		lda bresenham_err
		sec
		sbc bresenham_dy
		sta bresenham_err
		lda bresenham_x1
		clc
		adc bresenham_sx
		sta bresenham_x1
:
		; if err2 < dy:
		;   err = err + dx
		;   y = y + sy
		pla			; pop err2
		cmp bresenham_dy	; skip if err2 - dy >= 0
		bpl :+
		lda bresenham_err
		clc
		adc bresenham_dx
		sta bresenham_err
		lda bresenham_y1
		clc
		adc bresenham_sy
		sta bresenham_y1
:
		DEBUG_PREPIXEL_DRAW bresenham_x1, bresenham_y1
        PSET bresenham_x1, bresenham_y1
		DEBUG_PIXEL_DRAWN bresenham_x1, bresenham_y1
        lda bresenham_x1
        cmp bresenham_x2
        beq @checkX
        jmp drawLineStart

        @checkX:
        lda bresenham_y1
        cmp bresenham_y2
        beq @endDrawLine
        jmp drawLineStart

    @endDrawLine:
rts

_b11PSet:
sta @coY
jsr popa
sta @coX
DEBUG_PREPIXEL_DRAW @coX, @coY
PSET @coX, @coY
DEBUG_PIXEL_DRAWN @coX, @coY
rts
@coX: .byte $0
@coY: .byte $0

.segment "BANKRAMFLOOD"

PIC_DEFAULT = 16
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
FLOOD_QUEUE_END = $BEB1


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
.local @start
.local @q
.local @end
.local @incBank
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
bra @end

@incBank:
inc RAM_BANK ; The next flood bank will have identical code, so we can just increment the bank
inc _sposBank
@end:
.endmacro

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

.macro FLOOD_Q_RETRIEVE
.local @end
.local @serve
.local @resetBank
.local @returnResult
.local @serve

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

_bFloodQretrieve:
FLOOD_Q_RETRIEVE
rts
.endif