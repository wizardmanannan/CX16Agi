.include "x16.inc"

.ifndef OBJECT_INC
OBJECT_INC = 1

.import _b5FlushBuffer

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
.endif