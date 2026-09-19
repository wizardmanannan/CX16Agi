.include "x16.inc"

.ifndef OBJECT_INC
OBJECT_INC = 1

.import _b5FlushBuffer

.segment "BANKRAM0D"

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
_bDWriteNext:
ldx WRITE_ZP + 1
cpx #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write
ldx WRITE_ZP
cpx #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write

pha
lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1
TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer

lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1

pla

@write:
sta (WRITE_ZP)

inc WRITE_ZP
bne @return
inc WRITE_ZP + 1
@return:
rts

.endif