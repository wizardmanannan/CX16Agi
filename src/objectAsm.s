.include "x16.inc"

.ifndef OBJECT_INC
OBJECT_INC = 1

.import _b5FlushBuffer
.import _bDObjectNameLengths

.segment "BANKRAM0D"

.macro WRITE_NEXT
ldx WRITE_ZP + 1
cpx #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @highByteCheck

@write:
sta (WRITE_ZP)
inc WRITE_ZP
beq @incHigh
rts
@incHigh:
inc WRITE_ZP + 1
@return:
rts

@highByteCheck:
ldx WRITE_ZP
cpx #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write

@refreshBuffer:
pha
lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1
TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer

lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1
pla
jmp @write
.endmacro

; #define WRITE_NEXT(toWrite)  \
;     do {                              \
;        if(data >= GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE) \
; 		{ \
; 			b5FlushBuffer(&bufferStatus); \
;             data = GOLDEN_RAM_WORK_AREA; \
; 		} \
; 		 *data++ = toWrite; \
;         \
;     } while(0);

WRITE_ZP = ZP_TMP_10
BUFFER_STATUS_ADDRESS = ZP_TMP_12
OBJECT_NAME = ZP_TMP_13
CH = ZP_TMP_14
OBJ_NAME_OFFSET = ZP_TMP_14 + 1
I_COUNTER = ZP_TMP_16
J_COUNTER = ZP_TMP_16 + 1 
LASTLENGTH = ZP_TMP_17
THISLENGTH = ZP_TMP_17 + 1
_bDWriteNext:
WRITE_NEXT


_bDDisplayInventoryInner:
lda (OBJECT_NAME)
sta CH
beq @return

ldx #$0
ldy #$0
@loop:
lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @highByteCheck

@write:
sty OBJ_NAME_OFFSET
txa
tay
lda CH
sta (WRITE_ZP),y
ldy OBJ_NAME_OFFSET

iny
inx

@getObjectName:
lda (OBJECT_NAME),y
sta CH
bne @loop

clc
txa
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1
bra @return

@highByteCheck:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write

@refreshBuffer:
lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1

TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer

lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1

ldx #$0
jmp @write



@return:
rts

_bDDisplayInventoryInnerNoCompare:
.export _bDDisplayInventoryInnerNoCompare
lda (OBJECT_NAME)
beq return

ldx WRITE_ZP
stx @inventoryInnerWrite + 1
ldx WRITE_ZP + 1
stx @inventoryInnerWrite + 2

ldx #$0
ldy #$0
@loop:
@inventoryInnerWrite:
sta GOLDEN_RAM_WORK_AREA,x
iny
inx
@getObjectName:
lda (OBJECT_NAME),y
bne @loop

clc
txa
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1

return:
rts


bDPadWordsWithSpacesWithCompare:
; cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
; beq @checkLow
; bcs @checkLoopCondition ;Don't think we need this because i don't think j can be that big
; jmp bDPadWordsWithSpacesNoCompare
; @checkLow:
; lda sreg
; cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
; bcs @checkLoopCondition
; jmp bDPadWordsWithSpacesNoCompare

; bra @checkLoopCondition
@loop:
inc J_COUNTER
@checkLoopCondition:
lda J_COUNTER
cmp #TILES_ACROSS
bcs @endLoop

lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @lowByteCheckSpace

@writeSpace:
 lda #SPACE
 sta (WRITE_ZP)
 inc WRITE_ZP
 bne @loop

@incHighSpace:
inc WRITE_ZP + 1
bra @loop

@lowByteCheckSpace:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @writeSpace

lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1
TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer
;                 *WRITE_ZP_PTR = GOLDEN_RAM_WORK_AREA;
lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1
jmp @writeSpace;
@endLoop:
rts

_bDPadWordsWithSpaces:
sta LASTLENGTH
jsr popa 
sta I_COUNTER

@loopInit:
tay
lda _bDObjectNameLengths,y
clc
adc LASTLENGTH
sta J_COUNTER

clc
lda #<GOLDEN_RAM_WORK_AREA
adc J_COUNTER
sta sreg
lda #>GOLDEN_RAM_WORK_AREA
adc #$0

lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE - TILES_ACROSS)
bcs @checkLowByte

@noCompareNeeded:
ldy #$0
ldx J_COUNTER

@checkLoopCondition:
cpx #TILES_ACROSS
bcs @endLoop

@writeSpace:
lda #SPACE
sta (WRITE_ZP),y
iny
inx
bra @checkLoopCondition

@endLoop:

clc
tya
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1

rts

@checkLowByte:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE - TILES_ACROSS)
bcc @noCompareNeeded
jmp bDPadWordsWithSpacesWithCompare

.endif