.ifndef COMMAND_LOOP_INC
COMMAND_LOOP_INC = 1

 LOGIC_ENTRY_PARAMETERS_OFFSET = 0

.include "constants.asm"

; Zero Page
ZP_PTR_LE   = $02 
ZP_PTR_LF = $04
NO_ZERO_PAGE_VALUES = $4

oldZPValues: .byte $0,$0,$0,$0
currentLogicEntry: .byte $80
;currentLogicFile:  .byte $80
codePointer: .byte $0,$0
codeBank: .byte $0
startPos: .byte $0,$0
endPos: .byte $0,$0
_commandLoop:
    .export _commandLoop
    
    LDX #$0
    saveZPLoop:
    LDA ZP_PTR_LE,x
    STA oldZPValues,x
    inx 
    cpx #NO_ZERO_PAGE_VALUES
    bne saveZPLoop

    STA ZP_PTR_LE
    STX ZP_PTR_LE + 1

    LDA GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET
    LDX GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET + 1
    STA ZP_PTR_LF
    STX ZP_PTR_LF + 1

    LDY #LOGIC_FILE_LOGIC_CODE_OFFSET
    LDA (ZP_PTR_LE),y
    STA startPos
    LDY #LOGIC_FILE_LOGIC_CODE_OFFSET + 1
    LDA (ZP_PTR_LE),y
    STA startPos + 1

    STP
    LDX #$0
    restoreZPLoop:
    LDA oldZPValues,x
    STA ZP_PTR_LE,x
    inx 
    cpx #NO_ZERO_PAGE_VALUES
    bne restoreZPLoop

    rts
.endif