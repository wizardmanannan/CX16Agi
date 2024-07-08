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

NEW_LINE = 10

;Reserved For Interpreter
; Define some zero page pointers
ZP_PTR_CODE = $A9
;$08 is reserved for code window in codeWindow.s
ZP_TMP = $AB
ZP_PTR_CH  = $EF
ZP_PTR_B1  = $F1
ZP_PTR_B2  = $F3
ZP_PTR_DISP  = $F5

;Free to use
ZP_TMP_2 = $AD
ZP_TMP_3 = $AF
ZP_TMP_4 = $B1
ZP_TMP_5 = $B3
ZP_TMP_6 = $B5
ZP_TMP_7 = $B7
ZP_TMP_8 = $B9
ZP_TMP_9 = $BB
ZP_TMP_10 = $BD
ZP_TMP_12 = $BF
ZP_TMP_13 = $C1
ZP_TMP_14 = $C3
ZP_TMP_16 = $C5
ZP_TMP_17 = $C7
ZP_TMP_18 = $C9
ZP_TMP_19 = $DB
ZP_TMP_20 = $DD
ZP_TMP_21 = $DF
ZP_TMP_22 = $E1

;Float Division
ZP_DIV_AREA = $E3
ZP_DIV_BANK = $E5
ZP_DIV_ADDR = $FC 

ZP_PTR_LF = $E7
ZP_PTR_LE = $E9
ZP_PTR_PLF_HIGH = $EB
ZP_PTR_PLF_LOW = $ED

;Sprite Memory Manager These are 8 bit values
ZP_PTR_SEG_32 = $F7 
ZP_PTR_SEG_64 = $F8
ZP_PTR_HIGH_BYTE_START = $F9
ZP_PTR_WALL_32 = $FA
ZP_PTR_WALL_64 = $FB



; Define the starting address for golden RAM
GOLDEN_RAM = $400

; Define banks for various purposes
LOGIC_COMMANDS_BANK = $0F
HELPERS_BANK = $05
COMMAND_LOOP_HELPER_BANK = $0F
MEKA_BANK = $06
LOGIC_BANK = $3E
LOGIC_ENTRY_ADDRESSES_BANK = $6
LOGIC_CODE_BANK = $6
PICTURE_BANK = $11
PICTURE_CODE_OVERFLOW_BANK = $4
TEXT_BANK = $3
GRAPHICS_BANK = $6
SPRITE_MANAGER_BANK = $9
IRQ_BANK = $6
BANKED_ALLOC_BANK = $10
PARSER_BANK = $7
LINE_DRAWING_BANK = $8

FIRST_FLOOD_BANK = $27
NO_FLOOD_BANKS = $0A
LAST_FLOOD_BANK = FIRST_FLOOD_BANK + NO_FLOOD_BANKS - 1
SPRITE_UPDATES_BANK = $0E
SPLIT_BUFFER_BANK = $0C

DIVISION_METADATA_BANK = $31

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
LOGIC_LOADED_OFFSET = 0
LOGIC_ENTRY_POINT_OFFSET = 1
LOGIC_ENTRY_CURRENT_POINT_OFFSET = 3

; Define the JSRFAR kernal address
JSRFAR_KERNAL_ADDR = $FF6E

; Define true and false values for easier reading
TRUE = 1
FALSE = 0

;Define sizes
LOGIC_ENTRY_SIZE = 8
LOGIC_FILE_SIZE = 2

;Work Area

GOLDEN_RAM_WORK_AREA = $400 + LOCAL_WORK_AREA_GOLDEN_OFFSET
LOCAL_WORK_AREA_SIZE = 500
GOLDEN_RAM_WORK_AREA_END = GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE - 1

DISPLAY_SCALE     = 64 ; 2X zoom

COLOR_BLACK      = $000
COLOR_BLUE       = $00A
COLOR_GREEN      = $0A0
COLOR_CYAN       = $0AA
COLOR_RED        = $A00
COLOR_MAGENTA    = $A0A
COLOR_BROWN      = $A50
COLOR_LIGHT_GRAY = $AAA
COLOR_DARK_GRAY  = $555
COLOR_LIGHT_BLUE = $55F
COLOR_LIGHT_GREEN= $5F5
COLOR_LIGHT_CYAN = $5FF
COLOR_LIGHT_RED  = $F55
COLOR_LIGHT_MAGENTA = $F5F
COLOR_YELLOW     = $FF5
COLOR_WHITE      = $FFF


MULT_TABLE_HALF_POINT = $80

NEG_1_16 = $FFFF

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

; Macro for getting a 16-bit struct value
.macro   GET_STRUCT_16_STORED_OFFSET offset, pointer, result ;Where offset in stored in memory rather than constant
         LDY   offset
         LDA   (pointer),y
         STA   result
         INY
         LDA   (pointer),y
         STA   result + 1
.endmacro

.macro  SET_STRUCT_16_STORED_OFFSET_VALUE_IN_REG offset, pointer
         LDY  offset
         STA   (pointer),y
         TXA
         INY
         STA   (pointer),y

.endmacro

.macro  SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG offset, pointer
         LDY  offset
         STA   (pointer),y
.endmacro

; Macro for getting an 8-bit struct value
.macro   GET_STRUCT_8 offset, pointer, result
         LDY   #offset
         LDA   (pointer),y
         STA   result
.endmacro

; Macro for getting an 8-bit struct value
.macro   GET_STRUCT_8_STORED_OFFSET offset, pointer, result
         LDY   offset
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


;Load the value into a/x first, and then call this macro the number of times to multiply
;Output: low both a and y high: x

.macro PW2_MULT_16_CHAIN
clc ;Multiply By 2
asl
tay
txa
rol
tax
tya

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
       .local @end
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


; Macro for comparing two 16-bit words and branching if greater or equal
.macro GREATER_THAN_OR_EQ_16_LITERAL word1, literal, successBranch, failBranch
       .local @branch
       .local @end
       lda word1 + 1
       cmp #> literal
       .ifblank failBranch
       bcc @end
       .endif
       .ifnblank failBranch
       jmp failBranch
       .endif
       bne @branch
       lda word1
       cmp #< literal
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

; Macro for comparing two 16-bit words and branching if equal
.macro EQ_16_WORD_TO_LITERAL word1, word2, successBranch, failBranch
       .local @branch
       .local @end
       lda word1 + 1
       cmp #>word2
       bne @end
       lda word1
       cmp #< word2
       bne @end
       jmp successBranch
       @end:
       .ifnblank failBranch
       jmp failBranch
       .endif

.endmacro

.macro EQ_32_LONG_TO_LITERAL long1, word1, word2, successBranch, failBranch
.local @higherBits
EQ_16_WORD_TO_LITERAL long1, word1, @higherBits , failBranch
@higherBits:
EQ_16_WORD_TO_LITERAL long1 + 2, word2, successBranch , failBranch
.endmacro


;Macro for comparing two 16-bit words and branching if not equal
.macro NEQ_16_WORD_TO_LITERAL word1, word2, successBranch, failBranch
       .local @branch
       .local @end
       lda word1 + 1
       cmp #> word2
       bne successBranch
       lda word1
       cmp #< word2
       beq @end
       jmp successBranch
       @end:
       .ifnblank failBranch
       jmp failBranch
       .endif

.endmacro


; Macro for comparing two 8-bit words and branching if less or equal
.macro LESS_THAN_OR_EQ_8 word1, word2, successBranch, failBranch
       .local @branch
       .local @end
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
       .local @end  
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
       .local @end  
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
       .local @end    
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
       .local @end
       lda word1 + 1
       cmp word2 + 1
       beq @lowerBit
       bcc @branch
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

; Macro for comparing two 16-bit words and branching if less
.macro LESS_THAN_16 word1, word2, successBranch, failBranch
       .local @branch
       .local @lowerBit
       .local @end
       lda word1 + 1
       cmp word2 + 1
       bcc @branch
       .ifblank failBranch
       bne @end
       .endif
       .ifnblank failBranch
       bne failBranch
       .endif
       @lowerBit:
       lda word1
       cmp word2
       beq successBranch    
        .ifblank failBranch
       bcs @end
       .endif
       .ifnblank failBranch
       bcs failBranch
       .endif
       @branch:
       jmp successBranch
       @end:
       
.endmacro

.macro LESS_THAN_32 word1, word2, successBranch, failBranch
       .local @checkIfHigherBitsAreEqual
       .local @end

       LESS_THAN_16 word1 + 2, word2 + 2, successBranch, @checkIfHigherBitsAreEqual
       @checkIfHigherBitsAreEqual:
       lda word1 + 3
       cmp word2 + 3
       .ifblank failBranch
       bne @end
       .endif
       .ifnblank failBranch
       bne failBranch
       .endif


       lda word1 + 2
       cmp word2 + 2
        .ifblank failBranch
       bne @end
       .endif
       .ifnblank failBranch
       bne failBranch
       .endif

       LESS_THAN_16 word1, word2, successBranch, failBranch

       @end:
.endmacro

.macro INC_32 long
lda #$1
clc
adc long
sta long
lda #$0
adc long + 1
sta long + 1
lda #$0
adc long + 2
sta long + 2
lda #$0
adc long + 3
sta long + 3
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
php
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
plp
.endmacro

;Global IRQ
default_irq_vector: .addr 0
VSYNC_BIT            = $01

; Debugging related variables
_logDebugVal1: .byte $0
_logDebugVal2: .byte $0
_logDebugVal3: .byte $0
_logDebugVal4: .byte $0
_logDebugVal5: .byte $0
_logDebugVal6: .byte $0

.macro PRESERVE_REG
pha
txa
pha
tya
pha
.endmacro

.macro RESTORE_REG
pla
tay
pla
txa
pla
.endmacro

.macro REENABLE_INTERRUPTS
 lda VSYNC_BIT
 sta VERA_isr
 cli
.endmacro

.endif