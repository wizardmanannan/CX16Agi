
; This code is an assembly language implementation of various logic commands,
; functions, and macros for a game engine.
; The code is split into different sections, which are imported and included from various files.
; The engine also has debugging capabilities that can be enabled.

; Prevent multiple inclusions of this file
.ifndef  LOGICCOMMANDS_INC
LOGICCOMMANDS_INC = 1

; Include necessary files
.include "global.s"
.include "codeWindow.s"

.global endMainLoop
.global mainLoop
.global returnFromOpCodeTrue
.global returnFromOpCodeFalse

; Import various functions and variables from other files
; (functions related to game logic, drawing, animations, sound, text, etc.)
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

.import _b5ZeroOpCode
.import _b2Load_logics
.import _b2Load_logics_v
.import _b2Load_pic
.import _b2Draw_pic
.import _b2Show_pic
.import _b2Discard_pic
.import _b2Overlay_pic
.import _b2Show_pri_screen ;check
.import _b2Load_view
.import _b2Load_view_v
.import _b2Discard_view
.import _b2Animate_obj
.import _b2Unanimate_all
.import _b2Draw
.import _b2Erase
.import _b2Position
.import _b2Position_v
.import _b2Get_posn
.import _b2Reposition
.import _b2Set_view
.import _b2Set_view_v
.import _b2Set_loop
.import _b2Set_loop_v
.import _b2Fix_loop
.import _b2Release_loop
.import _b2Set_cel
.import _b2Set_cel_v
.import _b2Last_cel
.import _b2Current_cel
.import _b3Current_loop
.import _b3Current_view
.import _b3Number_of_loops
.import _b3Set_priority
.import _b3Set_priority_v
.import _b3Release_priority
.import _b3Get_priority
.import _b3Stop_update
.import _b3Start_update
.import _b3Force_update
.import _b3Ignore_horizon
.import _b3Observe_horizon
.import _b3Set_horizon
.import _b3Object_on_water
.import _b3Object_on_land
.import _b3Object_on_anything
.import _b3Ignore_objs
.import _b3Observe_objs
.import _b3Distance
.import _b3Stop_cycling
.import _b3Start_cycling
.import _b3Normal_cycle
.import _b3End_of_loop
.import _b3Reverse_cycle
.import _b3Reverse_loop
.import _b3Cycle_time
.import _b3Stop_motion
.import _b3Start_motion
.import _b3Step_size
.import _b3Step_time
.import _b3Move_obj
.import _b3Move_obj
.import _b3Move_obj_v
.import _b3Follow_ego
.import _b3Wander
.import _b3Normal_motion
.import _b3Set_dir
.import _b3Get_dir
.import _b3Ignore_blocks
.import _b3Observe_blocks
.import _b3Get
.import _b3Get_v
.import _b3Drop
.import _b3Put
.import _b3Put_v
.import _b3Get_room_v
.import _b3Load_sound
.import _b3Play_sound
.import _b3Stop_sound
.import _b3CharIsIn
.import _b3ProcessString
.import _b3Print
.import _b4Print_v
.import _b4Display
.import _b4Display_v
.import _b4Clear_lines
.import _b4Text_screen
.import _b4Graphics
.import _b4Set_cursor_char
.import _b4Set_text_attribute
.import _b4Configure_screen
.import _b4Status_line_on
.import _b4Status_line_off
.import _b4Set_string
.import _b4Get_string
.import _b4Word_to_string
.import _b4Parse
.import _b4Get_num
.import _b4Prevent_input
.import _b4Accept_input
.import _b4Set_key
.import _b4Add_to_pic
.import _b4Add_to_pic_v
.import _b4Status
.import _b4Restart_game
.import _b4Show_obj
.import _b4Random_num
.import _b4Program_control
.import _b4Player_control
.import _b4Obj_status_v
.import _b4Quit
.import _b4Pause
.import _b4Version
.import _b4Set_scan_start
.import _b4Reset_scan_start
.import _b4Reposition_to
.import _b4Reposition_to_v
.import _b4Print_at
.import _b4Print_at_v
.import _b4Discard_view_v
.import _b4Clear_text_rect
.import _b4Set_menu
.import _b4Set_menu_item
.import _b4Menu_input
.import _b4Show_obj_v
.import _b4Mul_n
.import _b4Mul_v
.import _b4Div_n
.import _b4Div_v

.import _executeLogic

.import _exitAllLogics
.import _hasEnteredNewRoom
.import _newRoomNum

; Debugging related imports (only included if DEBUG is defined)
.ifdef DEBUG
.import _debugGreaterThan_8N
.import _debugLessThan_8N
.import _debugGreaterThan_8V
.import _debugLessThan_8V
.import _debugIsSet
.import _debugEqualN
.import _debugEqualV
.import _debugInc
.import _debugDec
.import _debugAddN
.import _debugAddV
.import _debugSubN
.import _debugSubV
.import _debugAssignN
.import _debugPostCheckVar
.import _debugPostCheckFlag
.import _debugIndirect
.import _debugIndirectV
.import _debugNewRoom
.import _debugExitAllLogics
.import _stopAtFunc
.endif 



; Debugging related variables
_logDebugVal1: .byte $0
_logDebugVal2: .byte $0

; Debugging related macros (for debugging comparisons, flag checking, arithmetic operations, etc.)
.macro DEBUG_GREATER_THAN_8_N var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
JSRFAR _debugGreaterThan_8N, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_LESS_THAN_8_N var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugLessThan_8N, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_GREATER_THAN_8_V var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2
JSRFAR _debugGreaterThan_8V, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_LESS_THAN_8_V var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugLessThan_8V, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_IS_SET var1
.ifdef DEBUG
LOAD_CODE_WIN_CODE
sta _logDebugVal1

JSRFAR _debugIsSet, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_IS_EQUAL_N var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugEqualN, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_IS_EQUAL_V var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugEqualV, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_INC
.ifdef DEBUG
LOAD_CODE_WIN_CODE
sta _logDebugVal1

JSRFAR _debugInc, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_DEC
.ifdef DEBUG
LOAD_CODE_WIN_CODE
sta _logDebugVal1

lda #< _debugDec
ldx #> _debugDec

JSRFAR _debugDec, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_ADD_N var
.ifdef DEBUG
sta _logDebugVal2
lda var
sta _logDebugVal1

JSRFAR _debugAddN, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_ADD_V var
.ifdef DEBUG

JSRFAR _debugAddV, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_SUB_N var
.ifdef DEBUG
sta _logDebugVal2
lda var
sta _logDebugVal1

JSRFAR _debugSubN, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_SUB_V var
.ifdef DEBUG

lda #< _debugSubV
ldx #> _debugSubV

JSRFAR _debugSubV, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_ASSIGN_N var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

lda #< _debugAssignN
ldx #> _debugAssignN

JSRFAR _debugAssignN, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_ASSIGN_V var
.ifdef DEBUG

JSRFAR _debugAddV, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_POST_CHECK_VAR var
.ifdef DEBUG

.ifnblank var
lda var
sta _logDebugVal1
.endif

JSRFAR _debugPostCheckVar, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_POST_CHECK_FLAG var
.ifdef DEBUG
.ifnblank var
lda var
.endif
.ifblank var
lda _logDebugVal1
.endif

JSRFAR _debugPostCheckFlag, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_INDIRECT value
.ifdef DEBUG
lda value
sta _logDebugVal2

lda #< _debugIndirect
ldx #> _debugIndirect

JSRFAR _debugIndirect, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_INDIRECT_V value
.ifdef DEBUG
lda #<  _debugIndirectV
ldx #>  _debugIndirectV

JSRFAR _debugIndirectV, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_NEW_ROOM
.ifdef DEBUG
LOAD_CODE_WIN_CODE
sta _logDebugVal1

JSRFAR _debugNewRoom, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_EXIT_ALL_LOGICS
.ifdef DEBUG
JSRFAR _debugExitAllLogics, DEBUG_BANK
.endif
.endmacro

; Macro to store values on the stack for recursive calls
.macro STORE_ON_STACK_RECURSIVE_CALL
lda startPos
pha 
lda startPos + 1
pha
lda endPos
pha 
lda endPos + 1
pha
CATCH_UP_CODE
lda ZP_PTR_CODE
pha
lda ZP_PTR_CODE + 1
pha
lda _codeBank
pha
.endmacro

; Macro to restore values from the stack after a recursive call
.macro RESTORE_FROM_STACK_RECURSIVE_CALL
pla
sta _codeBank
pla
sta ZP_PTR_CODE + 1
pla
sta ZP_PTR_CODE
pla
sta endPos + 1
pla
sta endPos
pla
sta startPos + 1
pla
sta startPos

.endmacro

; Macro to get a variable or flag value and store it in the result
.macro SET_VAR_OR_FLAG areaStartOffset, toStore, var

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1

        .ifblank var
        LOAD_CODE_WIN_CODE
        .endif
        .ifnblank var
         lda var
        .endif

        adc ZP_TMP
        sta ZP_TMP 
        lda ZP_TMP + 1
        adc #$0

        lda toStore
        sta (ZP_TMP)
.endmacro

; Macro to get a variable or flag value with a variable offset and store it in the result
.macro GET_VAR_OR_FLAG areaStartOffset, result, var

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1
        .ifblank var
            LOAD_CODE_WIN_CODE
        .endif

        .ifnblank var
            lda var
        .endif


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

; Macro: HANDLE_C_IF_RESULT
.macro HANDLE_C_IF_RESULT
.local @success

; Branch if not equal
bne @success

; Jump to return address if false
jmp returnFromOpCodeFalse
@success:
; Jump to return address if true
jmp returnFromOpCodeFalse

.endmacro

; Macro: EXIT_ALL_LOGICS_IF_SET
.macro EXIT_ALL_LOGICS_IF_SET
DEBUG_EXIT_ALL_LOGICS
lda _exitAllLogics
beq @dontExit
; Jump to the end of the main loop if the flag is set
jmp endMainLoop
@dontExit:
.endmacro

; Main code segment
.segment "CODE"
callLogic: ; A subroutine for making calls, not an instruction
bra @start
@logNum: .byte $0
@previousBank: .byte $0
@start:
sta @logNum

lda RAM_BANK
sta @previousBank

STORE_ON_STACK_RECURSIVE_CALL

lda #MEKA_BANK
sta RAM_BANK
lda @logNum
ldx #$0
jsr _executeLogic
RESTORE_FROM_STACK_RECURSIVE_CALL
lda #TRUE
sta codeWindowInvalid
jsr refreshCodeWindow

lda @previousBank
sta RAM_BANK
rts

; Jump table for commands
; This table holds the addresses of various instructions.
; The interpreter will jump to the corresponding address based on the current command in the script.
jmpTableCommands1:
.addr endMainLoop
.addr b1Increment
.addr b1Decrement
.addr b1Assignn
.addr b1Assignv
.addr b1Addn
.addr b1Addv
.addr b2Subn
.addr b2Subv
.addr b2Lindirectv
.addr b2Rindirect
.addr b2Lindirectn
.addr b2Set
.addr b2Reset
.addr b2Toggle
.addr b2Setv
.addr b2Resetv
.addr b2Togglev
.addr b2New_room
.addr b2New_room_v
.addr b2Load_logicsCCall
.addr b2Load_logics_vCCall
.addr b2Call
.addr b2Call_v
.addr b2Load_picCCall
.addr b2Draw_picCCall
.addr b2Show_picCCall
.addr b2Discard_picCCall
.addr b2Overlay_picCCall
.addr b2Show_pri_screenCCall ;check
.addr b2Load_viewCCall
.addr b2Load_view_vCCall
.addr b2Discard_viewCCall
.addr b2Animate_objCCall
.addr b2Unanimate_allCCall
.addr b2DrawCCall
.addr b2EraseCCall
.addr b2PositionCCall
.addr b2Position_vCCall
.addr b2Get_posnCCall
.addr b2RepositionCCall
.addr b2Set_viewCCall
.addr b2Set_view_vCCall
.addr b2Set_loopCCall
.addr b2Set_loop_vCCall
.addr b2Fix_loopCCall
.addr b2Release_loopCCall
.addr b2Set_celCCall
.addr b2Set_cel_vCCall
.addr b2Last_celCCall
.addr b2Current_celCCall
.addr b3Current_loopCCall
.addr b3Current_viewCCall
.addr b3Number_of_loopsCCall
.addr b3Set_priorityCCall
.addr b3Set_priority_vCCall
.addr b3Release_priorityCCall
.addr b3Get_priorityCCall
.addr b3Stop_updateCCall
.addr b3Start_updateCCall
.addr b3Force_updateCCall
.addr b3Ignore_horizonCCall
.addr b3Observe_horizonCCall
.addr b3Set_horizonCCall
.addr b3Object_on_waterCCall
.addr b3Object_on_landCCall
.addr b3Object_on_anythingCCall
.addr b3Ignore_objsCCall
.addr b3Observe_objsCCall
.addr b3DistanceCCall
.addr b3Stop_cyclingCCall
.addr b3Start_cyclingCCall
.addr b3Normal_cycleCCall
.addr b3End_of_loopCCall
.addr b3Reverse_cycleCCall
.addr b3Reverse_loopCCall
.addr b3Cycle_timeCCall
.addr b3Stop_motionCCall
.addr b3Start_motionCCall
.addr b3Step_sizeCCall
.addr b3Step_timeCCall
.addr b3Move_objCCall
.addr b3Move_obj_vCCall
.addr b3Follow_egoCCall
.addr b3WanderCCall
.addr b3Normal_motionCCall
.addr b3Set_dirCCall
.addr b3Get_dirCCall
.addr b3Ignore_blocksCCall
.addr b3Observe_blocksCCall
.addr b1NoOp_4
.addr b1NoOp_0
.addr b3GetCCall
.addr b3Get_vCCall
.addr b3DropCCall
.addr b3PutCCall
.addr b3Put_vCCall
.addr b3Get_room_vCCall
.addr b3Load_soundCCall
.addr b3Play_soundCCall
.addr b3Stop_soundCCall
.addr b3PrintCCall
.addr b4Print_vCCall
.addr b4DisplayCCall
.addr b4Display_vCCall
.addr b4Clear_linesCCall
.addr b4Text_screenCCall
.addr b4GraphicsCCall
.addr b4Set_cursor_charCCall
.addr b4Set_text_attributeCCall
.addr b1NoOp_1
.addr b4Configure_screenCCall
.addr b4Status_line_onCCall
.addr b4Status_line_offCCall
.addr b4Set_stringCCall
.addr b4Get_stringCCall
.addr b4Word_to_stringCCall
.addr b4ParseCCall
.addr b4Get_numCCall
.addr b4Prevent_inputCCall
.addr b4Accept_inputCCall
.addr b4Set_keyCCall
.addr b4Add_to_picCCall
.addr b4Add_to_pic_vCCall
.addr b4StatusCCall
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_0

jmpTableCommands2:
.addr b4Restart_gameCCall
.addr b4Show_objCCall
.addr b4Random_numCCall
.addr b4Program_controlCCall
.addr b4Player_controlCCall
.addr b4Obj_status_vCCall
.addr b4QuitCCall
.addr b1NoOp_0
.addr _b4Pause
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_0
.addr _b4Version
.addr b1NoOp_1
.addr b1NoOp_1
.addr b1NoOp_1
.addr b4Set_scan_startCCall
.addr b4Reset_scan_startCCall
.addr b4Reposition_toCCall
.addr b4Reposition_to_vCCall
.addr b1NoOp_0
.addr b1NoOp_3
.addr b4Print_atCCall
.addr b4Print_at_vCCall
.addr _b4Discard_view_v
.addr _b4Clear_text_rect
.addr b1NoOp_2
.addr b4Set_menuCCall
.addr b4Set_menu_itemCCall
.addr b1NoOp_0
.addr b1NoOp_1
.addr b1NoOp_1
.addr b4Menu_inputCCall
.addr b4Show_obj_vCCall
.addr b1NoOp_0
.addr b1NoOp_0
.addr _b4Mul_n
.addr _b4Mul_v
.addr _b4Div_n
.addr _b4Div_v
.addr b1NoOp_0
.addr b1NoOp_1
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_1
.addr b1NoOp_1
.addr b1NoOp_0
.addr b1NoOp_1
.addr b1NoOp_0
.addr b1NoOp_4
.addr b1NoOp_2
.addr b1NoOp_0
.addr b1NoOp_0

; Variables used in the code
returnAddress: .word $0
var1: .byte $0
var2: .byte $0

; Banked RAM section
.segment "BANKRAM01"
; This section contains various instructions for the interpreter, such as:
; No operation, increment, decrement, assign, add, and subtract
; Each section of code is marked by a label, such as "b1NoOp_0", "b1Increment", and so on.

b1NoOp_0:
    jmp mainLoop

b1NoOp_1:
    INC_CODE
    jmp mainLoop

b1NoOp_2:
    INC_CODE_BY @amount
    jmp mainLoop
    @amount: .word $2

b1NoOp_3:
    INC_CODE_BY @amount
    jmp mainLoop
    @amount: .word $3

b1NoOp_4:
    INC_CODE_BY @amount
    jmp mainLoop
    @amount: .word $4

;Logic Commands
;Instruction 0 return is handled by jumping straight to the end main loop

b1Increment:
    bra @start
    @value: .byte $0
    @var: .byte $0
    @start:
         DEBUG_INC
         LOAD_CODE_WIN_CODE
         sta @var
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @value

         INC_CODE
         lda @value 
         cmp #$FF
         beq @end
         inc @value
         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @value, @var
         @end:        
         DEBUG_POST_CHECK_VAR
         jmp mainLoop


b1Decrement:
    bra @start
    @value: .byte $0
    @var: .byte $0
    @start:
         DEBUG_DEC
         LOAD_CODE_WIN_CODE
         sta @var
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @value

         INC_CODE
         lda @value 
         beq @end
         dec @value
         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @value, @var
         @end:        
         DEBUG_POST_CHECK_VAR
         jmp mainLoop

b1Assignn:
      bra @start
    @val: .byte $0
    @var: .byte $0
    @start:
         LOAD_CODE_WIN_CODE
         sta @var
         INC_CODE

         LOAD_CODE_WIN_CODE
         sta @val

         DEBUG_ASSIGN_N @var, @val

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var
         INC_CODE

        DEBUG_POST_CHECK_VAR @var

         jmp mainLoop

b1Assignv:
      bra @start
    @val: .byte $0
    @var: .byte $0
    @start:
         .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1  
         .endif

         LOAD_CODE_WIN_CODE
         sta @var

         INC_CODE
        
        

        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal2
         .endif
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val
         INC_CODE

         DEBUG_ASSIGN_V

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var


         DEBUG_POST_CHECK_VAR @var

         jmp mainLoop

b1Addn:
      bra @start
      @existingVal: .byte $0
      @resultVal: .byte $0
      @var: .byte $0
    @start:
         LOAD_CODE_WIN_CODE
         sta @var
         
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @existingVal
         INC_CODE
         LOAD_CODE_WIN_CODE
         
         DEBUG_ADD_N @var 

         clc
         adc @existingVal
         INC_CODE
         sta @resultVal

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @resultVal, @var

        DEBUG_POST_CHECK_VAR @var 
        
         jmp mainLoop

b1Addv:
      bra @start
      @existingVal: .byte $0
      @val: .byte $0
      @var: .byte $0
    @start:
         LOAD_CODE_WIN_CODE
         
        .ifdef DEBUG
            sta _logDebugVal1  
         .endif
         
         sta @var
         
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @existingVal
                  
         INC_CODE

         .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal2
         .endif

         DEBUG_ADD_V

         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val

         INC_CODE
         lda @val

         clc
         adc @existingVal
         INC_CODE
         sta @val

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var
        
         DEBUG_POST_CHECK_VAR @var

         jmp mainLoop

.segment "BANKRAM02"
b2Subn:
      bra @start
      @existingVal: .byte $0
      @val: .byte $0
      @var: .byte $0
    @start:
         LOAD_CODE_WIN_CODE
         sta @var
         
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @existingVal
         INC_CODE

         LOAD_CODE_WIN_CODE
         
         DEBUG_SUB_N @var

         sec
         sbc @existingVal
         INC_CODE
         sta @val

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var

         DEBUG_POST_CHECK_VAR @var

         jmp mainLoop

b2Subv:
      bra @start
      @existingVal: .byte $0
      @val: .byte $0
      @var: .byte $0
    @start:
         LOAD_CODE_WIN_CODE
         sta @var
         
        
         .ifdef DEBUG
            sta _logDebugVal1
         .endif

         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @existingVal
         INC_CODE
        
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val
         INC_CODE
         lda @val

         .ifdef DEBUG
            sta _logDebugVal2
         .endif

         DEBUG_SUB_V

         clc
         sbc @existingVal
         INC_CODE
         sta @val

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var
         
        DEBUG_POST_CHECK_VAR @var

         jmp mainLoop

b2Lindirectv:
      bra @start
      @varNum1: .byte $0
      @varNum2: .byte $0
      @result: .byte $0
    @start:         
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif
            
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @varNum1
         INC_CODE

         .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal2
         .endif

         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @varNum2
         INC_CODE

         DEBUG_INDIRECT_V
        
         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @varNum2, @varNum1
         
         DEBUG_POST_CHECK_VAR @varNum1

         jmp mainLoop


b2Rindirect:
      bra @start
      @var1: .byte $0
      @var2: .byte $0
      @result: .byte $0
    @start:         
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var1
         INC_CODE
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var2
         INC_CODE
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var2
         INC_CODE
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @result, @var2
         INC_CODE

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @result, @var1

         jmp mainLoop

b2Lindirectn:
      bra @start
      @varNum: .byte $0
      @val: .byte $0
    @start:        
        
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif
            
         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @varNum
         INC_CODE
         LOAD_CODE_WIN_CODE
         sta @val
         INC_CODE

         DEBUG_INDIRECT @val
        
         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @varNum
         
         DEBUG_POST_CHECK_VAR @varNum

         jmp mainLoop

b2Set:   
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif

         SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$1
                  
         INC_CODE

        DEBUG_POST_CHECK_FLAG

         jmp mainLoop

b2Reset:  
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif

         SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$0
         INC_CODE

        DEBUG_POST_CHECK_FLAG

         jmp mainLoop

b2Toggle:
      bra @start
      @flagVal: .byte $0
      @start:
        GET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, @flagVal
                
        lda @flagVal
        bne @setTrue
        stz @flagVal
        bra @setValue
        
        @setTrue:
        lda #$1
        sta @flagVal
        @setValue:
        SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, @flagVal

        INC_CODE

        DEBUG_POST_CHECK_FLAG @flagVal

        jmp mainLoop

b2Setv:
      bra @start
      @result: .byte $0
      @start:
        GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @result
                
        SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$1, @result

        INC_CODE

        DEBUG_POST_CHECK_FLAG @result

        jmp mainLoop

b2Resetv:
      bra @start
      @result: .byte $0
      @start:
        
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif

        GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @result
                
        SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$0, @result

        INC_CODE

        DEBUG_POST_CHECK_FLAG

        jmp mainLoop

b2Togglev:
      bra @start
      @varVal: .byte $0
      @flagVal: .byte $0
      @start:
        GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @varVal
        GET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, @flagVal, @varVal
                
        lda @flagVal
        bne @setTrue
        stz @flagVal
        bra @setValue
        
        @setTrue:
        lda #$1
        sta @flagVal
        @setValue:
        SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @flagVal

        DEBUG_POST_CHECK_FLAG @flagVal

        INC_CODE
        jmp mainLoop

b2Call:
LOAD_CODE_WIN_CODE
jsr callLogic
INC_CODE
EXIT_ALL_LOGICS_IF_SET
jmp mainLoop

b2Call_v:
bra @start
@var1: .byte $0
@start:
GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var1

lda @var1
jsr callLogic
INC_CODE
EXIT_ALL_LOGICS_IF_SET
jmp mainLoop

b2New_room:
DEBUG_NEW_ROOM
LOAD_CODE_WIN_CODE
sta _newRoomNum
stz _newRoomNum + 1
switchToNewRoom:
lda #TRUE
sta _hasEnteredNewRoom
sta _exitAllLogics
INC_CODE 
jmp endMainLoop


b2New_room_v:
bra @start
@var1: .byte $0
@start:
GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var1
bra switchToNewRoom

.segment "BANKRAM02"
b2Load_logicsCCall:
        jsr _b2Load_logics
        jmp mainLoop
b2Load_logics_vCCall:
        jsr _b2Load_logics_v
        jmp mainLoop
b2Load_picCCall:
        jsr _b2Load_pic
        jmp mainLoop
b2Draw_picCCall:
        jsr _b2Draw_pic
        jmp mainLoop
b2Show_picCCall:
        jsr _b2Show_pic
        jmp mainLoop
b2Discard_picCCall:
        jsr _b2Discard_pic
        jmp mainLoop
b2Overlay_picCCall:
        jsr _b2Overlay_pic
        jmp mainLoop
b2Show_pri_screenCCall: ;check
        jsr _b2Show_pri_screen ;check
        jmp mainLoop
b2Load_viewCCall:
        jsr _b2Load_view
        jmp mainLoop
b2Load_view_vCCall:
        jsr _b2Load_view_v
        jmp mainLoop
b2Discard_viewCCall:
        jsr _b2Discard_view
        jmp mainLoop
b2Animate_objCCall:
        jsr _b2Animate_obj
        jmp mainLoop
b2Unanimate_allCCall:
        jsr _b2Unanimate_all
        jmp mainLoop
b2DrawCCall:
        jsr _b2Draw
        jmp mainLoop
b2EraseCCall:
        jsr _b2Erase
        jmp mainLoop
b2PositionCCall:
        jsr _b2Position
        jmp mainLoop
b2Position_vCCall:
        jsr _b2Position_v
        jmp mainLoop
b2Get_posnCCall:
        jsr _b2Get_posn
        jmp mainLoop
b2RepositionCCall:
        jsr _b2Reposition
        jmp mainLoop
b2Set_viewCCall:
        jsr _b2Set_view
        jmp mainLoop
b2Set_view_vCCall:
        jsr _b2Set_view_v
        jmp mainLoop
b2Set_loopCCall:
        jsr _b2Set_loop
        jmp mainLoop
b2Set_loop_vCCall:
        jsr _b2Set_loop_v
        jmp mainLoop
b2Fix_loopCCall:
        jsr _b2Fix_loop
        jmp mainLoop
b2Release_loopCCall:
        jsr _b2Release_loop
        jmp mainLoop
b2Set_celCCall:
        jsr _b2Set_cel
        jmp mainLoop
b2Set_cel_vCCall:
        jsr _b2Set_cel_v
        jmp mainLoop
b2Last_celCCall:
        jsr _b2Last_cel
        jmp mainLoop
b2Current_celCCall:
        jsr _b2Current_cel
        jmp mainLoop
.segment "BANKRAM03"
b3Current_loopCCall:
        jsr _b3Current_loop
        jmp mainLoop
b3Current_viewCCall:
        jsr _b3Current_view
        jmp mainLoop
b3Number_of_loopsCCall:
        jsr _b3Number_of_loops
        jmp mainLoop
b3Set_priorityCCall:
        jsr _b3Set_priority
        jmp mainLoop
b3Set_priority_vCCall:
        jsr _b3Set_priority_v
        jmp mainLoop
b3Release_priorityCCall:
        jsr _b3Release_priority
        jmp mainLoop
b3Get_priorityCCall:
        jsr _b3Get_priority
        jmp mainLoop
b3Stop_updateCCall:
        jsr _b3Stop_update
        jmp mainLoop
b3Start_updateCCall:
        jsr _b3Start_update
        jmp mainLoop
b3Force_updateCCall:
        jsr _b3Force_update
        jmp mainLoop
b3Ignore_horizonCCall:
        jsr _b3Ignore_horizon
        jmp mainLoop
b3Observe_horizonCCall:
        jsr _b3Observe_horizon
        jmp mainLoop
b3Set_horizonCCall:
        jsr _b3Set_horizon
        jmp mainLoop
b3Object_on_waterCCall:
        jsr _b3Object_on_water
        jmp mainLoop
b3Object_on_landCCall:
        jsr _b3Object_on_land
        jmp mainLoop
b3Object_on_anythingCCall:
        jsr _b3Object_on_anything
        jmp mainLoop
b3Ignore_objsCCall:
        jsr _b3Ignore_objs
        jmp mainLoop
b3Observe_objsCCall:
        jsr _b3Observe_objs
        jmp mainLoop
b3DistanceCCall:
        jsr _b3Distance
        jmp mainLoop
b3Stop_cyclingCCall:
        jsr _b3Stop_cycling
        jmp mainLoop
b3Start_cyclingCCall:
        jsr _b3Start_cycling
        jmp mainLoop
b3Normal_cycleCCall:
        jsr _b3Normal_cycle
        jmp mainLoop
b3End_of_loopCCall:
        jsr _b3End_of_loop
        jmp mainLoop
b3Reverse_cycleCCall:
        jsr _b3Reverse_cycle
        jmp mainLoop
b3Reverse_loopCCall:
        jsr _b3Reverse_loop
        jmp mainLoop
b3Cycle_timeCCall:
        jsr _b3Cycle_time
        jmp mainLoop
b3Stop_motionCCall:
        jsr _b3Stop_motion
        jmp mainLoop
b3Start_motionCCall:
        jsr _b3Start_motion
        jmp mainLoop
b3Step_sizeCCall:
        jsr _b3Step_size
        jmp mainLoop
b3Step_timeCCall:
        jsr _b3Step_time
        jmp mainLoop
b3Move_objCCall:
        jsr _b3Move_obj
        jmp mainLoop
b3Move_obj_vCCall:
        jsr _b3Move_obj_v
        jmp mainLoop
b3Follow_egoCCall:
        jsr _b3Follow_ego
        jmp mainLoop
b3WanderCCall:
        jsr _b3Wander
        jmp mainLoop
b3Normal_motionCCall:
        jsr _b3Normal_motion
        jmp mainLoop
b3Set_dirCCall:
        jsr _b3Set_dir
        jmp mainLoop
b3Get_dirCCall:
        jsr _b3Get_dir
        jmp mainLoop
b3Ignore_blocksCCall:
        jsr _b3Ignore_blocks
        jmp mainLoop
b3Observe_blocksCCall:
        jsr _b3Observe_blocks
        jmp mainLoop
b3GetCCall:
        jsr _b3Get
        jmp mainLoop
b3Get_vCCall:
        jsr _b3Get_v
        jmp mainLoop
b3DropCCall:
        jsr _b3Drop
        jmp mainLoop
b3PutCCall:
        jsr _b3Put
        jmp mainLoop
b3Put_vCCall:
        jsr _b3Put_v
        jmp mainLoop
b3Get_room_vCCall:
        jsr _b3Get_room_v
        jmp mainLoop
b3Load_soundCCall:
        jsr _b3Load_sound
        jmp mainLoop
b3Play_soundCCall:
        jsr _b3Play_sound
        jmp mainLoop
b3Stop_soundCCall:
        jsr _b3Stop_sound
        jmp mainLoop
b3PrintCCall:
        jsr _b3Print
        jmp mainLoop
.segment "BANKRAM04"
b4Print_vCCall:
        jsr _b4Print_v
        jmp mainLoop
b4DisplayCCall:
        jsr _b4Display
        jmp mainLoop
b4Display_vCCall:
        jsr _b4Display_v
        jmp mainLoop
b4Clear_linesCCall:
        jsr _b4Clear_lines
        jmp mainLoop
b4Text_screenCCall:
        jsr _b4Text_screen
        jmp mainLoop
b4GraphicsCCall:
        jsr _b4Graphics
        jmp mainLoop
b4Set_cursor_charCCall:
        jsr _b4Set_cursor_char
        jmp mainLoop
b4Set_text_attributeCCall:
        jsr _b4Set_text_attribute
        jmp mainLoop
b4Configure_screenCCall:
        jsr _b4Configure_screen
        jmp mainLoop
b4Status_line_onCCall:
        jsr _b4Status_line_on
        jmp mainLoop
b4Status_line_offCCall:
        jsr _b4Status_line_off
        jmp mainLoop
b4Set_stringCCall:
        jsr _b4Set_string
        jmp mainLoop
b4Get_stringCCall:
        jsr _b4Get_string
        jmp mainLoop
b4Word_to_stringCCall:
        jsr _b4Word_to_string
        jmp mainLoop
b4ParseCCall:
        jsr _b4Parse
        jmp mainLoop
b4Get_numCCall:
        jsr _b4Get_num
        jmp mainLoop
b4Prevent_inputCCall:
        jsr _b4Prevent_input
        jmp mainLoop
b4Accept_inputCCall:
        jsr _b4Accept_input
        jmp mainLoop
b4Set_keyCCall:
        jsr _b4Set_key
        jmp mainLoop
b4Add_to_picCCall:
        jsr _b4Add_to_pic
        jmp mainLoop
b4Add_to_pic_vCCall:
        jsr _b4Add_to_pic_v
        jmp mainLoop
b4StatusCCall:
        jsr _b4Status
        jmp mainLoop
b4Restart_gameCCall:
        jsr _b4Restart_game
        jmp mainLoop
b4Show_objCCall:
        jsr _b4Show_obj
        jmp mainLoop
b4Random_numCCall:
        jsr _b4Random_num
        jmp mainLoop
b4Program_controlCCall:
        jsr _b4Program_control
        jmp mainLoop
b4Player_controlCCall:
        jsr _b4Player_control
        jmp mainLoop
b4Obj_status_vCCall:
        jsr _b4Obj_status_v
        jmp mainLoop
b4QuitCCall:
        jsr _b4Quit
        jmp mainLoop
b4PauseCCall:
        jsr _b4Pause
        jmp mainLoop
b4VersionCCall:
        jsr _b4Version
        jmp mainLoop
b4Set_scan_startCCall:
        jsr _b4Set_scan_start
        jmp mainLoop
b4Reset_scan_startCCall:
        jsr _b4Reset_scan_start
        jmp mainLoop
b4Reposition_toCCall:
        jsr _b4Reposition_to
        jmp mainLoop
b4Reposition_to_vCCall:
        jsr _b4Reposition_to_v
        jmp mainLoop
b4Print_atCCall:
        jsr _b4Print_at
        jmp mainLoop
b4Print_at_vCCall:
        jsr _b4Print_at_v
        jmp mainLoop
b4Discard_view_vCCall:
        jsr _b4Discard_view_v
        jmp mainLoop
b4Clear_text_rectCCall:
        jsr _b4Clear_text_rect
        jmp mainLoop
b4Set_menuCCall:
        jsr _b4Set_menu
        jmp mainLoop
.segment "BANKRAM05"
b4Set_menu_itemCCall:
        jsr _b4Set_menu_item
        jmp mainLoop
b4Menu_inputCCall:
        jsr _b4Menu_input
        jmp mainLoop
b4Show_obj_vCCall:
        jsr _b4Show_obj_v
        jmp mainLoop
b4Mul_nCCall:
        jsr _b4Mul_n
        jmp mainLoop
b4Mul_vCCall:
        jsr _b4Mul_v
        jmp mainLoop
b4Div_nCCall:
        jsr _b4Div_n
        jmp mainLoop
b4Div_vCCall:
        jsr _b4Div_v
        jmp mainLoop



;If Checks
.segment "BANKRAM0F"
jmpTableIf:
.addr b1NoOp_0
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

b1Equaln:

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE
    
    DEBUG_IS_EQUAL_N _logDebugVal1, var2
    lda var1
    cmp var2
    beq @success
    jmp returnFromOpCodeFalse
    @success:
    jmp returnFromOpCodeTrue

b1Equalv:

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal2
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_IS_EQUAL_V _logDebugVal1, _logDebugVal2

    lda var1
    cmp var2

    beq @success
    jmp returnFromOpCodeFalse
    @success:
    jmp returnFromOpCodeTrue

b1Lessn:

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    DEBUG_LESS_THAN_8_N _logDebugVal1, var2
    LESS_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Lessv:
    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal2
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_LESS_THAN_8_V _logDebugVal1, _logDebugVal2
    LESS_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Greatern:
    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    DEBUG_GREATER_THAN_8_N _logDebugVal1, var2
    
    GREATER_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Greaterv:
    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal1
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    .ifdef DEBUG
    LOAD_CODE_WIN_CODE
    sta _logDebugVal2
    .endif

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_GREATER_THAN_8_V _logDebugVal1, _logDebugVal2
    
    GREATER_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Isset:
    GET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, var1

    DEBUG_IS_SET

    INC_CODE
    lda var1
    beq @fail
    jmp returnFromOpCodeTrue
    @fail:
    jmp returnFromOpCodeFalse

b1Issetv:
    GET_VAR_OR_FLAG_VAR_OFFSET FLAGS_AREA_START_GOLDEN_OFFSET, var1

    DEBUG_IS_SET

    INC_CODE
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
.endif
