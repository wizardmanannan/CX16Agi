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

.ifdef DEBUG
.import _debugGreaterThan_8
.import _debugLessThan_8
.import _debugIsSet
.import _debugEqual
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

_logDebugVal1: .byte $0
_logDebugVal2: .byte $0
.endif 

.macro DEBUG_GREATER_THAN_8 var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugGreaterThan_8, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_LESS_THAN_8 var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugLessThan_8, DEBUG_BANK

.endif
.endmacro

.macro DEBUG_IS_SET var1
.ifdef DEBUG
LOAD_CODE_WIN_CODE
sta _logDebugVal1

JSRFAR _debugIsSet, DEBUG_BANK
.endif
.endmacro

.macro DEBUG_IS_EQUAL var1, var2
.ifdef DEBUG
lda var1
sta _logDebugVal1
lda var2
sta _logDebugVal2

JSRFAR _debugEqual, DEBUG_BANK
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

.macro GET_VAR_OR_FLAG areaStartOffset, result, var

        clc
        lda #<GOLDEN_RAM
        adc #<areaStartOffset
        sta ZP_TMP
        lda #>GOLDEN_RAM
        adc #>areaStartOffset
        sta ZP_TMP + 1
        nop
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

.macro HANDLE_C_IF_RESULT
.local @success

bne @success

jmp returnFromOpCodeFalse
@success:
jmp returnFromOpCodeFalse

.endmacro

.macro EXIT_ALL_LOGICS_IF_SET
lda _exitAllLogics
beq @dontExit
jmp mainLoop
@dontExit:
.endmacro

.segment "CODE"
callLogic: ;A subroutine for making calls not an instruction
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
.addr _b2Load_logics
.addr _b2Load_logics_v
.addr b2Call
.addr b2Call_v
.addr _b2Load_pic
.addr _b2Draw_pic
.addr _b2Show_pic
.addr _b2Discard_pic
.addr _b2Overlay_pic
.addr _b2Show_pri_screen ;check
.addr _b2Load_view
.addr _b2Load_view_v
.addr _b2Discard_view
.addr _b2Animate_obj
.addr _b2Unanimate_all
.addr _b2Draw
.addr _b2Erase
.addr _b2Position
.addr _b2Position_v
.addr _b2Get_posn
.addr _b2Reposition
.addr _b2Set_view
.addr _b2Set_view_v
.addr _b2Set_loop
.addr _b2Set_loop_v
.addr _b2Fix_loop
.addr _b2Release_loop
.addr _b2Set_cel
.addr _b2Set_cel_v
.addr _b2Last_cel
.addr _b2Current_cel
.addr _b3Current_loop
.addr _b3Current_view
.addr _b3Number_of_loops
.addr _b3Set_priority
.addr _b3Set_priority_v
.addr _b3Release_priority
.addr _b3Get_priority
.addr _b3Stop_update
.addr _b3Start_update
.addr _b3Force_update
.addr _b3Ignore_horizon
.addr _b3Observe_horizon
.addr _b3Set_horizon
.addr _b3Object_on_water
.addr _b3Object_on_land
.addr _b3Object_on_anything
.addr _b3Ignore_objs
.addr _b3Observe_objs
.addr _b3Distance
.addr _b3Stop_cycling
.addr _b3Start_cycling
.addr _b3Normal_cycle
.addr _b3End_of_loop
.addr _b3Reverse_cycle
.addr _b3Reverse_loop
.addr _b3Cycle_time
.addr _b3Stop_motion
.addr _b3Start_motion
.addr _b3Step_size
.addr _b3Step_time
.addr _b3Move_obj
.addr _b3Move_obj_v
.addr _b3Follow_ego
.addr _b3Wander
.addr _b3Normal_motion
.addr _b3Set_dir
.addr _b3Get_dir
.addr _b3Ignore_blocks
.addr _b3Observe_blocks
.addr b1NoOp_4
.addr b1NoOp_0
.addr _b3Get
.addr _b3Get_v
.addr _b3Drop
.addr _b3Put
.addr _b3Put_v
.addr _b3Get_room_v
.addr _b3Load_sound
.addr _b3Play_sound
.addr _b3Stop_sound
.addr _b3Print
.addr _b4Print_v
.addr _b4Display
.addr _b4Display_v
.addr _b4Clear_lines
.addr _b4Text_screen
.addr _b4Graphics
.addr _b4Set_cursor_char
.addr _b4Set_text_attribute
.addr b1NoOp_1
.addr _b4Configure_screen
.addr _b4Status_line_on
.addr _b4Status_line_off
.addr _b4Set_string
.addr _b4Get_string
.addr _b4Word_to_string
.addr _b4Parse
.addr _b4Get_num
.addr _b4Prevent_input
.addr _b4Accept_input
.addr _b4Set_key
.addr _b4Add_to_pic
.addr _b4Add_to_pic_v
.addr _b4Status
.addr b1NoOp_0
.addr b1NoOp_0
.addr b1NoOp_0

jmpTableCommands2:
.addr _b4Restart_game
.addr _b4Show_obj
.addr _b4Random_num
.addr _b4Program_control
.addr _b4Player_control
.addr _b4Obj_status_v
.addr _b4Quit
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
.addr _b4Set_scan_start
.addr _b4Reset_scan_start
.addr _b4Reposition_to
.addr _b4Reposition_to_v
.addr b1NoOp_0
.addr b1NoOp_3
.addr _b4Print_at
.addr _b4Print_at_v
.addr _b4Discard_view_v
.addr _b4Clear_text_rect
.addr b1NoOp_2
.addr _b4Set_menu
.addr _b4Set_menu_item
.addr b1NoOp_0
.addr b1NoOp_1
.addr b1NoOp_1
.addr _b4Menu_input
.addr _b4Show_obj_v
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



returnAddress: .word $0
var1: .byte $0
var2: .byte $0

.segment "BANKRAM01"
b1NoOp_0:
    jmp _afterLogicCommand

b1NoOp_1:
    nop
    INC_CODE
    jmp _afterLogicCommand

b1NoOp_2:
    INC_CODE_BY #$2
    jmp _afterLogicCommand

b1NoOp_3:
    INC_CODE_BY #$3
    jmp _afterLogicCommand

b1NoOp_4:
    INC_CODE_BY #$4
    jmp _afterLogicCommand

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
         jmp _afterLogicCommand


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
         jmp _afterLogicCommand

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

         jmp _afterLogicCommand

b1Assignv:
      bra @start
    @val: .byte $0
    @var: .byte $0
    @start:
         .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1  
         .endif

         GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val

         INC_CODE

        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal2
         .endif

         DEBUG_ASSIGN_V

         SET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @val, @var
         INC_CODE

         DEBUG_POST_CHECK_VAR @var

         jmp _afterLogicCommand

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
        
         jmp _afterLogicCommand

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

         jmp _afterLogicCommand

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

         jmp _afterLogicCommand

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

         jmp _afterLogicCommand

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

         jmp _afterLogicCommand


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

         jmp _afterLogicCommand

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

         jmp _afterLogicCommand

b2Set:   
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif

         SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$1
                  
         INC_CODE

        DEBUG_POST_CHECK_FLAG

         jmp _afterLogicCommand

b2Reset:  
        .ifdef DEBUG
            LOAD_CODE_WIN_CODE
            sta _logDebugVal1
         .endif

         SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$0
         INC_CODE

        DEBUG_POST_CHECK_FLAG

         jmp _afterLogicCommand

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

        jmp _afterLogicCommand

b2Setv:
      bra @start
      @result: .byte $0
      @start:
        GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @result
                
        SET_VAR_OR_FLAG FLAGS_AREA_START_GOLDEN_OFFSET, #$1, @result

        INC_CODE

        DEBUG_POST_CHECK_FLAG @result

        jmp _afterLogicCommand

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

        jmp _afterLogicCommand

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
        jmp _afterLogicCommand

b2Call:
LOAD_CODE_WIN_CODE
jsr callLogic
INC_CODE
EXIT_ALL_LOGICS_IF_SET
jmp _afterLogicCommand

b2Call_v:
bra @start
@var1: .byte $0
@start:
EXIT_ALL_LOGICS_IF_SET
GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, @var1

lda @var1
jsr callLogic
INC_CODE
jmp _afterLogicCommand

b2New_room:
LOAD_CODE_WIN_CODE
sta _newRoomNum

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

;If Checks
.segment "BANKRAM05"
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
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE
    
    DEBUG_IS_EQUAL var1, var2

    lda var1
    cmp var2
    beq @success
    jmp returnFromOpCodeFalse
    @success:
    jmp returnFromOpCodeTrue

b1Equalv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_IS_EQUAL var1, var2

    lda var1
    cmp var2

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

    DEBUG_LESS_THAN_8 var1, var2
    LESS_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Lessv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_LESS_THAN_8 var1, var2
    LESS_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Greatern:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    LOAD_CODE_WIN_CODE
    sta var2
    INC_CODE

    DEBUG_GREATER_THAN_8 var1, var2
    
    GREATER_THAN_8 var1, var2, returnFromOpCodeTrue, returnFromOpCodeFalse

b1Greaterv:
    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var1
    INC_CODE

    GET_VAR_OR_FLAG VARS_AREA_START_GOLDEN_OFFSET, var2
    INC_CODE

    DEBUG_GREATER_THAN_8 var1, var2

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
