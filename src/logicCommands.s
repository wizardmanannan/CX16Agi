.ifndef  LOGICCOMMANDS_INC
LOGICCOMMANDS_INC = 1
.include "global.s"
.include "commandLoop.s"

returnAddress: .word $0
var1: .byte $0
var2: .byte $0
address: .word $0
_b1Greatern:
    ADD_WORD_8_IND_16 ZP_PTR_IF_CODE_WIN, GOLDEN_RAM + VARS_AREA_START_GOLDEN_OFFSET, address
    
    ldy #$0

    lda < (address),y
    iny
    lda > (address),y
    stp
    jmp returnFromOpCode
.endif
