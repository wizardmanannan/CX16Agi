; Check if global definitions are included, if not, include them
.ifndef  PICTURE_INC
PICTURE_INC = 1

.include "x16.inc"
.include "global.s"
.include "graphics.s"

.import _picColour
.import _picDrawEnabled

.segment "CODE"

DEBUG_CHECK_LINE_DRAWN = 1


.ifdef DEBUG_PICTURE
.import _b5DebugPixelDraw
.import _b5DebugPrePixelDraw
.import _b5CheckPixelDrawn
.endif

.ifdef DEBUG_CHECK_LINE_DRAWN
.import _b5LineDrawDebug
.endif

_pixelCounter: .word $0 ; Used for debugging but can't be hidden in the if def as the C won't compile without it.

.macro DEBUG_PIXEL_DRAW var1, var2
.ifdef DEBUG_PICTURE
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
JSRFAR _b5DebugPixelDraw, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_PREPIXEL_DRAW var1, var2
.ifdef DEBUG_CHECK_DRAWN
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
inc _pixelCounter
JSRFAR _b5DebugPrePixelDraw, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_PIXEL_DRAWN var1, var2
.ifdef DEBUG_CHECK_DRAWN
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
JSRFAR _b5CheckPixelDrawn, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_LINE_DRAW var1, var2, var3, var4
.ifdef DEBUG_CHECK_LINE_DRAWN
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


_drawWhere: .word $0
_toDraw: .byte $0
.macro PSET coX, coY
.local @endPSet
.local @lowerNumbers
.local @start
.local @originalZPTMP
.local @checkYBounds

lda coX
cmp #160        ; if x > 159
bcc @checkYBounds         ; then @end
stp
lda #$1
jmp @endPSet

@checkYBounds:
lda coY
cmp #168        ; if y > 167
bcc @start         ; then @end
stp
lda #$2
jmp @endPSet

@originalZPTMP: .word $0

@start:
lda _picDrawEnabled
bne @drawPictureScreen         ; If picDrawEnabled == 0, skip to the end
jmp @endPSet

@drawPictureScreen:
lda ZP_TMP 
sta @originalZPTMP  ; Save ZP_TMP
lda ZP_TMP+1
sta @originalZPTMP+1

lda _picColour
asl a           ; Shift left 4 times to multiply by 16
ora _picColour
sta _toDraw     ; toDraw = picColour << 4 | picColour

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

lda _toDraw
sta VERA_data0

DEBUG_PIXEL_DRAW coX, coY

@endPSet:
    ; Continue with the rest of the code

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
_bresenham_x1: .byte $0
_bresenham_x2: .byte $0
_bresenham_y1: .byte $0
_bresenham_y2: .byte $0
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
		lda _bresenham_y1
		sec
		sbc _bresenham_y2	; A = y1 - y2
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
		lda _bresenham_x1
		sec
		sbc _bresenham_x2	; A = x1 - x2
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
		DEBUG_LINE_DRAW _bresenham_x1, _bresenham_y1, _bresenham_x2, _bresenham_y2
		DEBUG_PREPIXEL_DRAW _bresenham_x1, _bresenham_y1
        PSET _bresenham_x1, _bresenham_y1
		DEBUG_PIXEL_DRAWN _bresenham_x1, _bresenham_y1
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
		lda _bresenham_x1
		clc
		adc bresenham_sx
		sta _bresenham_x1
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
		lda _bresenham_y1
		clc
		adc bresenham_sy
		sta _bresenham_y1
:
		DEBUG_PREPIXEL_DRAW _bresenham_x1, _bresenham_y1
        PSET _bresenham_x1, _bresenham_y1
		DEBUG_PIXEL_DRAWN _bresenham_x1, _bresenham_y1
        lda _bresenham_x1
        cmp _bresenham_x2
        beq @checkX
        jmp drawLineStart

        @checkX:
        lda _bresenham_y1
        cmp _bresenham_y2
        beq @endDrawLine
        jmp drawLineStart

    @endDrawLine:
rts

.endif