.ifndef  LOGICCOMMANDS_INC
LOGICCOMMANDS_INC = 1
.include "global.s"
.include "commandLoop.s"
.include "codeWindow.s"

.macro LOAD_CODE_WIN_CODE
        ldy cwCurrentCode
        lda (ZP_PTR_CODE_WIN),y
.endmacro

.macro GET_VAR_OR_FLAG areaStartOffset, result

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1

        LOAD_CODE_WIN_CODE

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
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    LESS_THAN_OR_EQ_8 var2, var1, returnFromOpCodeFalse, returnFromOpCodeTrue

.endif
