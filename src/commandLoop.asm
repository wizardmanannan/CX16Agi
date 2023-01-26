.ifndef COMMAND_LOOP_INC
COMMAND_LOOP_INC = 1

 LOGIC_ENTRY_PARAMETERS_OFFSET = 0

.include "constants.asm"

currentLogicEntry: .byte $0,$0
currentLogicFile: .byte $0,$0
codePointer: .byte $0,$0
codeBank: .byte $0
startPos: .byte $0
endPos: .byte $0
_commandLoop:
    .export _commandLoop
    STP
    STA currentLogicFile
    STX currentLogicFile + 1

    LDA GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET
    LDX GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET + 1
    STA currentLogicEntry
    STX currentLogicEntry + 1

    

    rts
.endif