.ifndef  LOGICCOMMANDS_INC
LOGICCOMMANDS_INC = 1
.include "global.s"
.include "commandLoop.s"
.include "codeWindow.s"

.import _b1Has
.import _b1Obj_in_room
.import _b1Posn
.import _b1Controller
.import _b1Have_key 
.import _b1Said
.import _b1Compare_strings
.import _b1Obj_in_box
.import _b1Center_posn
.import _b1Right_posn

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

.macro GET_VAR_OR_FLAG_VAR_OFFSET areaStartOffset, result

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1

        GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, result

        adc ZP_TMP
        sta ZP_TMP 
        lda ZP_TMP + 1
        adc #$0

        lda (ZP_TMP)

        sta result
.endmacro

.macro HANDLE_C_IF_RESULT
.local @success

bne @success

jmp returnFromOpCodeFalse
@success:
jmp returnFromOpCodeFalse

.endmacro

.segment "BANKRAM05"
b1Equaln:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    lda var2

    beq @success
    jmp returnFromOpCodeFalse
    @success:
    jmp returnFromOpCodeTrue

b1Equalv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    lda var2

    beq @success
    jmp returnFromOpCodeFalse
    @success:
    jmp returnFromOpCodeTrue

b1Lessn:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    GREATER_THAN_OR_EQ_8 var2, var1, returnFromOpCodeFalse, returnFromOpCodeTrue

b1Lessv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    GREATER_THAN_OR_EQ_8 var2, var1, returnFromOpCodeFalse, returnFromOpCodeTrue


b1Greatern:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE
    
    LESS_THAN_OR_EQ_8 var2, var1, returnFromOpCodeFalse, returnFromOpCodeTrue

b1Greaterv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    LESS_THAN_OR_EQ_8 var2, var1, returnFromOpCodeFalse, returnFromOpCodeTrue

b1Isset:
    GET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE
    lda var1
    beq @fail
    jmp returnFromOpCodeTrue
    @fail:
    jmp returnFromOpCodeFalse

b1Issetv:
    GET_VAR_OR_FLAG_VAR_OFFSET FLAGS_AREA_START_GOLDEN_OFFSET, var1
    lda var1
    bne @fail
    jmp returnFromOpCodeTrue
    @fail:
    jmp returnFromOpCodeFalse

b1HasCCall:
   jsr _b1Has
   HANDLE_C_IF_RESULT

b1Obj_in_roomCCall:
   jsr _b1Obj_in_room
   HANDLE_C_IF_RESULT

b1PosnCCall:
   jsr _b1Posn
   HANDLE_C_IF_RESULT

b1ControllerCCall:
    jsr _b1Controller
    HANDLE_C_IF_RESULT

b1Have_keyCCall:
    jsr _b1Have_key
    HANDLE_C_IF_RESULT

b1SaidCCall:
    jsr _b1Said
    HANDLE_C_IF_RESULT

b1Compare_stringsCCall:
    jsr _b1Compare_strings
    HANDLE_C_IF_RESULT

b1Obj_in_boxCCall:
    jsr _b1Obj_in_box
    HANDLE_C_IF_RESULT

b1Center_posnCCall:
    jsr _b1Center_posn
    HANDLE_C_IF_RESULT

b1Right_posnCCall:
    jsr _b1Right_posn
    HANDLE_C_IF_RESULT

.segment "CODE"
jmpTableIf:
.word $0
.addr b1Equaln
.addr b1Equalv
.addr b1Lessn
.addr b1Lessv
.addr b1Greatern
.addr b1Greaterv
.addr b1Isset
.addr b1Issetv
.addr b1HasCCall
.addr b1Obj_in_roomCCall
.addr b1PosnCCall
.addr b1ControllerCCall
.addr b1Have_keyCCall
.addr b1SaidCCall
.addr b1Compare_stringsCCall
.addr b1Obj_in_boxCCall
.addr b1Center_posnCCall
.addr b1Right_posnCCall


jmpTableCommands:
.word $0



returnAddress: .word $0
var1: .byte $0
var2: .byte $0

.endif
