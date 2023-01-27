.ifndef  COMMAND_LOOP_INC
COMMAND_LOOP_INC = 1
LOGIC_ENTRY_PARAMETERS_OFFSET =  0
CODE_WINDOW_SIZE = 10

.include "global.s"

ZP_PTR_LF = $02
ZP_PTR_LE = $04
ZP_PTR_CODE = $06
ZP_PTR_CODE_WIN = $08
NO_ZERO_PAGE_VALUES = $4

oldZPValues: .byte $0,$0,$0,$0
codeBank: .byte $0
startPos: .word $0
endPos:  .word $0
entryPoint: .word $0
codeSize: .word $0
stillExecuting: .byte $1
_commandLoop:
    .export _commandLoop
         sta   ZP_PTR_LF
         stx   ZP_PTR_LF  + 1

         lda   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET
         ldx   GOLDEN_RAM + PARAMETERS_WORK_AREA_GOLDEN_OFFSET + LOGIC_ENTRY_PARAMETERS_OFFSET + 1
         sta   ZP_PTR_LE
         stx   ZP_PTR_LE  + 1
        
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_OFFSET, ZP_PTR_LF, startPos
         GET_STRUCT_16 LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET, ZP_PTR_LF, codeSize
         GET_STRUCT_8 LOGIC_FILE_LOGIC_BANK_OFFSET, ZP_PTR_LF, codeBank
         GET_STRUCT_16 LOGIC_ENTRY_POINT_OFFSET, ZP_PTR_LE, entryPoint
         
         ADD_WORD_16 startPos,entryPoint,ZP_PTR_CODE
         ADD_WORD_16 startPos,codeSize,endPos
         
         lda codeBank
         sta RAM_BANK
         mainLoop:
         GREATER_THAN_OR_EQ_16 ZP_PTR_CODE_WIN, endPos, endLoop
         lda stillExecuting
         cmp #TRUE
         bne endLoop

         BYTES_TO_STACK ZP_PTR_CODE, CODE_WINDOW_SIZE, ZP_PTR_CODE_WIN

         jmp endLoop ;temp line

         jmp mainLoop
         endLoop:
         CLEAR_STACK CODE_WINDOW_SIZE
         rts
.endif