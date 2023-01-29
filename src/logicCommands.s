.ifndef  LOGICCOMMANDS_INC
LOGICCOMMANDS_INC = 1
.include "global.s"
.include "commandLoop.s"

.macro GET_VAR_OR_FLAG areaStartOffset, ptrNum, result

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1

        lda (ptrNum)
        adc ZP_TMP
        sta ZP_TMP 
        lda ZP_TMP + 1
        adc #$0

        lda (ZP_TMP)
        sta result
.endmacro

jmpTableIf:
.word $0
.word $1
.word $2
.word $3
.word $4
.addr _b1Greatern

returnAddress: .word $0
var1: .byte $0
var2: .byte $0

_b1Greatern:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, ZP_PTR_IF_CODE_WIN, var1
    INC_MEM ZP_PTR_IF_CODE_WIN

    stp
    lda (ZP_PTR_CODE_WIN)
    sta var2
    INC_MEM ZP_PTR_IF_CODE_WIN
    
    LESS_THAN_OR_EQ_16 var2, var1, returnFromOpCodeTrue, returnFromOpCodeFalse

    ; ldy #$0

    ; lda < (address),y
    ; iny
    ; lda > (address),y
.endif
