.ifndef  GLOBAL_INC
GLOBAL_INC = 1

ZP_PTR_CODE_WIN = $08
ZP_PTR_IF_CODE_WIN = $10

GOLDEN_RAM = $400
RAM_BANK = $0
STACK_HIGH = $1

VARS_AREA_START_GOLDEN_OFFSET = 0
LOCAL_WORK_AREA_GOLDEN_OFFSET = 514
PARAMETERS_WORK_AREA_GOLDEN_OFFSET = 1015

LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET =  0
LOGIC_FILE_LOGIC_CODE_OFFSET =  2
LOGIC_FILE_LOGIC_BANK_OFFSET = 7

LOGIC_ENTRY_POINT_OFFSET = 1
LOGIC_ENTRY_CURRENT_POINT_OFFSET = 3

TRUE = 1
FALSE = 0

.macro  INC_MEM address
         clc
         lda address
         adc #$1
         sta address

         lda address + 1
         adc #$0
         sta address + 1
.endmacro

.macro   SET_STRUCT_16 offset, pointer, value
         LDA value
         LDY   #offset
         STA   (pointer),y
         LDA value + 1
         LDY   #offset + 1
         STA   (pointer),y
.endmacro

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
         @saveZPLoop:
         LDA   firstPointer,x
         STA   saveLocation,x
         inx
         cpx   #noValues
         bne   @saveZPLoop

         PLA
         TAX
         PLA

.endmacro

.macro   RESTORE_ZERO_PAGE firstPointer, saveLocation, noValues
         LDX   #$0
         @restoreZPLoop:
         LDA   saveLocation,x
         STA   firstPointer,x
         inx
         cpx   noValues
         bne   @restoreZPLoop

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

.macro ADD_WORD_8_IND_16 firstAddress, secondAddress, result
        clc
        lda (firstAddress)
        adc secondAddress
        sta result
        lda #$0
        adc secondAddress + 1
        sta result + 1
.endmacro

.macro SUB_WORD_16 firstAddress, secondAddress, result
        sec
        lda firstAddress
        sbc secondAddress
        sta result
        lda firstAddress + 1
        sbc secondAddress + 1
        sta result + 1
.endmacro

.macro SUB_WORD_16_IND firstAddress, secondAddress, offset, pointer
        sec
        lda firstAddress
        sbc secondAddress
        
        ldy   #offset
        sta   (pointer),y
        
        lda firstAddress + 1
        sbc secondAddress + 1
                
        iny
        sta   (pointer),y
.endmacro

.macro GREATER_THAN_OR_EQ_16 word1, word2, branchLabel
       .local @branch
       lda word1 + 1
       cmp word2 + 1
       bcc @end
       bne @branch
       lda word1
       cmp word2
       beq @branch
       bcs @branch
       bcs @end
       @branch:
       jmp branchLabel
       @end:

.endmacro

.macro BYTES_TO_STACK startAddress, copySize, addressFirst
        .local @startBtsLoop
        .local @endBtsLoop
        ldy #copySize
        dey
        @startBtsLoop:
        cpy #copySize
        bcs @endBtsLoop

        lda (startAddress),y
        pha

        dey
        jmp @startBtsLoop
        @endBtsLoop:
        tsx
        inx
        stx addressFirst
        lda #STACK_HIGH
        sta addressFirst + 1
.endmacro

.macro CLEAR_STACK clearSize
        .local @startClLoop
        .local @endClLoop
        ldx #$0
        @startClLoop:
        cpx #clearSize
        beq @endClLoop
        pla
        inx 
        jmp @startClLoop
        @endClLoop:
.endmacro
.endif
