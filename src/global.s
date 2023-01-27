.ifndef  GLOBAL_INC
GLOBAL_INC = 1

GOLDEN_RAM = $400
RAM_BANK = $0
STACK_HIGH = $1

LOCAL_WORK_AREA_GOLDEN_OFFSET = 514
PARAMETERS_WORK_AREA_GOLDEN_OFFSET = 1015

LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET =  0
LOGIC_FILE_LOGIC_CODE_OFFSET =  2
LOGIC_FILE_LOGIC_BANK_OFFSET = 7

LOGIC_ENTRY_POINT_OFFSET = 1

TRUE = 1
FALSE = 0

.macro   GET_STRUCT_16 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
         LDY   #offset + 1
         LDA   (pointer),y
         STA   result + 1
.endmacro

.macro   GET_STRUCT_8 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
.endmacro

.macro   SAVE_ZERO_PAGE firstPointer, saveLocation, noValues
         PHA
         TXA
         PHA

         LDX   #$0
         saveZPLoop:
         LDA   firstPointer,x
         STA   saveLocation,x
         inx
         cpx   #noValues
         bne   saveZPLoop

         PLA
         TAX
         PLA

.endmacro

.macro   RESTORE_ZERO_PAGE firstPointer, saveLocation, noValues
         LDX   #$0
         restoreZPLoop:
         LDA   saveLocation,x
         STA   firstPointer,x
         inx
         cpx   noValues
         bne   restoreZPLoop

.endmacro

.macro ADD_WORD_16 firstAddress, secondAddress, result
        clc
        lda firstAddress
        adc secondAddress
        sta result
        lda firstAddress + 1
        adc secondAddress + 1
        sta result + 1
.endmacro

.macro GREATER_THAN_OR_EQ_16 word1, word2, branchLabel
       lda word1 + 1
       cmp word2 + 1
       bcc @end
       bne branchLabel
       lda word1
       cmp word2
       beq branchLabel
       bcs branchLabel
       @end:

.endmacro

.macro BYTES_TO_STACK startAddress, copySize, addressFirst
        ldy #copySize
        dey
        startLoop:
        cpy #copySize
        bcs @end

        lda (startAddress),y
        pha

        dey
        jmp startLoop
        @end:
        stp
        tsx
        inx
        stx addressFirst
        lda #STACK_HIGH
        sta addressFirst + 1
.endmacro

.macro CLEAR_STACK clearSize
ldx #$0
@startLoop:
cpx #clearSize
beq @endLoop
stp
pla
inx 
jmp @startLoop
@endLoop:
.endmacro
.endif
