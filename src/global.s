; Check if global definitions are included, if not, include them
.ifndef  GLOBAL_INC

; Include the x16.inc file
.include "x16.inc"
;DEBUG = 0

; Define some start and end positions and code bank
startPos: .word $0
endPos:  .word $0
_codeBank: .byte $0

; Set global definitions
GLOBAL_INC = 1

; Define some zero page pointers
ZP_PTR_CODE = $69
;$08 is reserved for code window in codeWindow.s
ZP_TMP = $66
ZP_PTR_LF = $74
ZP_PTR_LE = $76
ZP_PTR_PLF_HIGH = $78
ZP_PTR_PLF_LOW = $80
ZP_PTR_CH  = $F8
ZP_PTR_B1  = $FA
ZP_PTR_B2  = $FC
ZP_PTR_DISP  = $FE
;System Reserved 82 and 83
; Set up zero page pointer (ZP_PTR_CODE_WIN) and other variables related to code window management.
ZP_PTR_CODE_WIN = $84 ;Zero Pointer Page Pointer To Code Window

; Define the starting address for golden RAM
GOLDEN_RAM = $400

; Define banks for various purposes
LOGIC_COMMANDS_BANK = $0F
DEBUG_BANK = $05
COMMAND_LOOP_HELPER_BANK = $0F
MEKA_BANK = $07
LOGIC_BANK = $3C
LOGIC_ENTRY_ADDRESSES_BANK = $8

; Define offsets for different areas within golden RAM
VARS_AREA_START_GOLDEN_OFFSET = 0
FLAGS_AREA_START_GOLDEN_OFFSET = 257
LOCAL_WORK_AREA_GOLDEN_OFFSET = 514
PARAMETERS_WORK_AREA_GOLDEN_OFFSET = 1015

; Define offsets for logic file and code
LOGIC_FILE_LOGIC_CODE_SIZE_OFFSET =  0
LOGIC_FILE_LOGIC_CODE_OFFSET =  2
LOGIC_FILE_LOGIC_DATA_OFFSET = 5
LOGIC_FILE_LOGIC_BANK_OFFSET = 7

; Define offsets for logic entry points
LOGIC_ENTRY_POINT_OFFSET = 1
LOGIC_ENTRY_CURRENT_POINT_OFFSET = 3

; Define the JSRFAR kernal address
JSRFAR_KERNAL_ADDR = $FF6E

; Define true and false values for easier reading
TRUE = 1
FALSE = 0

;Define struct sizes
LOGIC_ENTRY_SIZE = 8
LOGIC_FILE_SIZE = 2

; Macro for reading from an array
.macro READ_ARRAY_POINTER arrayZeroPointer
    clc
    asl 
    tay
    lda (arrayZeroPointer),y
    sta ZP_PTR_LE
    iny
    lda (arrayZeroPointer),y
    sta ZP_PTR_LE + 1
.endmacro

; Macro for setting a 16-bit struct value
.macro   SET_STRUCT_16 offset, pointer, value
         LDA value
         LDY   #offset
         STA   (pointer),y
         LDA value + 1
         LDY   #offset + 1
         STA   (pointer),y
.endmacro

; Macro for getting a 16-bit struct value
.macro   GET_STRUCT_16 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
         LDY   #offset + 1
         LDA   (pointer),y
         STA   result + 1
.endmacro

; Macro for getting an 8-bit struct value
.macro   GET_STRUCT_8 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
.endmacro

; Macro for saving zero page values
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

; Macro for restoring zero page values
.macro   RESTORE_ZERO_PAGE firstPointer, saveLocation, noValues
         LDX   #$0
         @restoreZPLoop:
         LDA   saveLocation,x
         STA   firstPointer,x
         inx
         cpx   noValues
         bne   @restoreZPLoop

.endmacro


; Macro for adding two 16-bit words
.macro ADD_WORD_16 firstAddress, secondAddress, result
        clc
        lda firstAddress
        adc secondAddress
        sta result
        lda firstAddress + 1
        adc secondAddress + 1
        sta result + 1
.endmacro

; Macro for adding a 16-bit word and an 8-bit word
.macro ADD_WORD_16_8 firstAddress, secondAddress, result
        clc
        lda firstAddress
        adc secondAddress
        sta result
        lda firstAddress + 1
        adc #$0
        sta result + 1
.endmacro

; Macro for subtracting two 16-bit words
.macro SUB_WORD_16 firstAddress, secondAddress, result
        sec
        lda firstAddress
        sbc secondAddress
        sta result
        lda firstAddress + 1
        sbc secondAddress + 1
        sta result + 1
.endmacro

; Macro for subtracting two 16-bit words with an indirect addressing mode
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

; Macro for comparing two 16-bit words and branching if greater or equal
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

; Macro for comparing two 8-bit words and branching if less or equal
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

; Macro for comparing two 8-bit words and branching if greater or equal
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

; Macro for comparing two 8-bit words and branching if greater
.macro GREATER_THAN_8 word1, word2, successBranch, failBranch
       .local @branch
              
       lda word1
       cmp word2
       
       beq @end
       bcs @branch
       bra @end

       @branch:
        jmp successBranch
       @end:
       .ifnblank failBranch
       jmp failBranch
       .endif
       
.endmacro

; Macro for comparing two 8-bit words and branching if less
.macro LESS_THAN_8 word1, word2, successBranch, failBranch
       .local @branch
              
       lda word1
       cmp word2
       
       beq @end
       bcc @branch
       bra @end

       @branch:
        jmp successBranch
       @end:
       .ifnblank failBranch
       jmp failBranch
       .endif
       
.endmacro

; Macro for comparing two 16-bit words and branching if less or equal
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

; Macro for left shifting a 16-bit word by 8 bits
.macro LEFT_SHIFT_BY_8 word1, result
;Shift by 8
lda word1
sta result + 1

; Zero the lower byte
lda #$00
sta result
.endmacro


; Macro for left shifting a 16-bit word by 1 bit
.macro LEFT_SHIFT_BY_1 word1, result
clc
lda word1
rol
sta result
lda word1 + 1
rol
sta result + 1
.endmacro


; Macro for bitwise OR operation on two 16-bit words
.macro ORA_16 word1, word2, result
lda word1
ora word2
sta result

lda word1 + 1
ora word2 + 1
sta result + 1
.endmacro

.macro STOP_AT_FUNC
pha
txa
pha
tya
pha
jsr _stopAtFunc
pla
tay
pla
tax
pla
.endmacro

.endif