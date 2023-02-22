.ifndef  GLOBAL_INC

codeBank: .byte $0

GLOBAL_INC = 1

ZP_PTR_CODE = $06
;$08 is reserved for code window in codeWindow.s
ZP_TMP = $10

GOLDEN_RAM = $400
RAM_BANK = $0

LOGIC_COMMANDS_BANK = $01

VARS_AREA_START_GOLDEN_OFFSET = 0
FLAGS_AREA_START_GOLDEN_OFFSET = 257
LOCAL_WORK_AREA_GOLDEN_OFFSET = 514
PARAMETERS_WORK_AREA_GOLDEN_OFFSET = 1015

LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET =  0
LOGIC_FILE_LOGIC_CODE_OFFSET =  2
LOGIC_FILE_LOGIC_BANK_OFFSET = 7

LOGIC_ENTRY_POINT_OFFSET = 1
LOGIC_ENTRY_CURRENT_POINT_OFFSET = 3

TRUE = 1
FALSE = 0

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

.macro ADD_WORD_16_8 firstAddress, secondAddress, result
        clc
        lda firstAddress
        adc secondAddress
        sta result
        lda firstAddress + 1
        adc #$0
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

.macro GREATER_THAN_OR_EQ_16 word1, word2, successBranch, failBranch
       .local @branch
       lda word1 + 1
       cmp word2 + 1
       .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif
       bne @branch
       lda word1
       cmp word2
       beq @branch
       bcs @branch
       .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif
       @branch:
       jmp successBranch
       @end:

.endmacro

.macro LESS_THAN_OR_EQ_8 word1, word2, successBranch, failBranch
       .local @branch
              
       lda word1
       cmp word2
       
       bcc @branch
       beq @branch
      
      .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif

       @branch:
       jmp successBranch
       @end:
       
.endmacro

.macro GREATER_THAN_OR_EQ_8 word1, word2, successBranch, failBranch
       .local @branch
              
       lda word1
       cmp word2
       
       bcs @branch
       bra @end

       @branch:
        jmp successBranch
       @end:
       .ifnblank failBranch
       jmp failBranch
       .endif
       
.endmacro

.macro LESS_THAN_OR_EQ_16 word1, word2, successBranch, failBranch
       .local @branch

       lda word1 + 1
       cmp word2 + 1
       bcc @lowerBit
       .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif
       @lowerBit:
       lda word1
       cmp word2
       beq @branch
       bcc @branch
       .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif
       @branch:
       jmp successBranch
       @end:
       
.endmacro

.macro LEFT_SHIFT_16 word1, amount, result
        .local @startLoop
        .local @endLoop

        ldx #$0
        @startLoop:
        cpx amount
        beq @endLoop

        clc
        lda word1
        rol
        sta result

        lda word1 + 1
        rol
        sta result + 1

        inx 
        bra @startLoop
        @endLoop:
.endmacro


.macro ORA_16 word1, word2, result
lda word1
ora word2
sta result

lda word1 + 1
ora word2 + 1
sta result + 1
.endmacro

.endif
