.ifndef MOVEMENT_INC
MOVEMENT_INC = 1

.include "global.s"

; Importing offsets for struct fields to be used in movement logic
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfParam1
.import _offsetOfParam2
.import _offsetOfParam3
.import _offsetOfParam4
.import _offsetOfDirection
.import _offsetOfMotion
.import _offsetOfStopped
.import _offsetOfStepSize
.import _getViewTab


.macro READ_DIRECTION_TABLE
asl
tax
lda newdir_row_addresses,x
sta sreg
lda newdir_row_addresses + 1,x
sta sreg + 1

ldy MVT_DIR_VAL_X
lda (sreg),y
.endmacro

.segment "BANKRAM0A"

;void bAMoveTo(ViewTable* localViewTab, byte entryNum);
_bAMoveTo:
.scope
MVT_LOCAL_VIEW_TAB = ZP_TMP_2
MVT_ENTRY_NUM = ZP_TMP_3
MVT_DIR_VAL_X = ZP_TMP_3 + 1
MVT_DIR = ZP_TMP_4

sta MVT_ENTRY_NUM
jsr popax 
sta MVT_LOCAL_VIEW_TAB
stx MVT_LOCAL_VIEW_TAB + 1

ldy _offsetOfParam1
lda (MVT_LOCAL_VIEW_TAB),y
sta sreg
ldy _offsetOfXPos
lda (MVT_LOCAL_VIEW_TAB),y
tax
ldy _offsetOfStepSize
lda (MVT_LOCAL_VIEW_TAB),y
tay
txa
jsr bADirectionIndex
stx MVT_DIR_VAL_X


ldy _offsetOfParam2
lda (MVT_LOCAL_VIEW_TAB),y
sta sreg
ldy _offsetOfYPos
lda (MVT_LOCAL_VIEW_TAB),y
tax
ldy _offsetOfStepSize
lda (MVT_LOCAL_VIEW_TAB),y
tay
txa
jsr bADirectionIndex
txa

READ_DIRECTION_TABLE

ldy _offsetOfDirection
sta (MVT_LOCAL_VIEW_TAB),y
sta MVT_DIR 

ldx MVT_ENTRY_NUM
bne @endMoveObj

@updateEgoDirection:

txa
lda #EGODIR
SET_VAR_NON_INTERPRETER sreg

@endMoveObj:
lda MVT_DIR
cmp #$0
bne @return

lda MVT_LOCAL_VIEW_TAB
ldx MVT_LOCAL_VIEW_TAB + 1
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1
lda MVT_ENTRY_NUM
sta VIEW_POS_ENTRY_NUM

TRAMPOLINE #VIEW_TAB_BANK, b9EndMoveObj

@return:
.endscope
rts

;Local refers to the reference to current viewTab
;------------------------------------------------------------------------------
; bADirectionIndex
; Inputs:
;   a: Local position (y coordinate)
;   y: Step size (motion threshold)
;   sreg: Ego position (y coordinate)
; Outputs:
;   a: Distance between local and ego positions
;   x: Direction of movement
; Description:
;   Determines the movement direction based on relative positions and step size.
;   Returns the distance and direction between the local position and ego position.
;------------------------------------------------------------------------------
bADirectionIndex:
    cmp sreg               ; Compare local position with ego position
    bcc @localIsSmaller    ; Branch if local is smaller than ego

@localIsBiggerEq:
    sec                    ; Set carry for subtraction
    sbc sreg               ; Calculate distance: local - ego

    sty sreg               ; Save ego position to sreg
    cmp sreg               ; Compare distances for movement decision
    bcc @dontMove          ; If local position is closer, don't move

    ldx #$0                ; Direction: No movement
    bra @end

@dontMove:
    ldx #$1                ; Direction: Towards ego
    bra @end

@localIsSmaller:
    ldx sreg               ; Save ego position in X register
    sta sreg               ; Update sreg with local position
    txa                    ; Transfer X to A
    sec                    ; Set carry for subtraction
    sbc sreg               ; Calculate distance: ego - local
    sty sreg               ; Save result to sreg
    cmp sreg               ; Compare distances
    beq @successLocalIsSmaller ; Check if movement is successful
    bcc @dontMove

@successLocalIsSmaller:
    ldx #$2                ; Direction: Away from ego
@end:
    rts                    ; Return from subroutine

;------------------------------------------------------------------------------
; Direction mapping table
; Encodes relative movement directions in a 3x3 grid:
;   Row 1: Directions for X paired with -Y
;   Row 2: Directions for X paired with 0 Y
;   Row 3: Directions for X paired with +Y
;------------------------------------------------------------------------------
newdir:
    .byte 8, 1, 2          ; Row 1 (top-left, top-center, top-right)
    .byte 7, 0, 3          ; Row 2 (mid-left, center, mid-right)
    .byte 6, 5, 4          ; Row 3 (bottom-left, bottom-center, bottom-right)

;------------------------------------------------------------------------------
; Row addresses for the direction mapping table
; Points to the start of each row in the direction table.
;------------------------------------------------------------------------------
newdir_row_addresses:
    .addr newdir           ; Row 1: Top row (-Y offsets)
    .addr newdir + 3       ; Row 2: Middle row (0 Y offset)
    .addr newdir + 6       ; Row 3: Bottom row (+Y offsets)

MAX_DIRECTION = 8          ; Maximum possible direction value

;------------------------------------------------------------------------------
; bAFollowEgoAsmSec
; Purpose:
;   Adjusts the local view table's position and direction to follow the ego.
; Inputs:
;   localViewTab: Pointer to the local view table
;   egoViewTab: Pointer to the ego view table
;   egoWidth: Width of the ego object
;   localCelWidth: Width of the local object
;------------------------------------------------------------------------------
_bAFollowEgoAsmSec:
.scope
    ; Zero-page temporary variables
    MVT_LOCAL_VIEW_TAB = ZP_TMP_2
    MVT_EGO_VIEW_TAB = ZP_TMP_3
    MVT_EGO_WIDTH = ZP_TMP_4
    MVT_LOCAL_CEL_WIDTH = ZP_TMP_4 + 1
    MVT_ECX = ZP_TMP_5
    MVT_LCX = ZP_TMP_5 + 1
    MVT_DIFF_X = ZP_TMP_6
    MVT_DIFF_Y = ZP_TMP_6 + 1
    MVT_DELTA = ZP_TMP_7
    MVT_DIR_VAL_X = ZP_TMP_8
    MVT_DIR_VAL_Y = ZP_TMP_8 + 1
    MVT_DIR = ZP_TMP_9

    sta MVT_LOCAL_CEL_WIDTH ; Save localCelWidth to zero-page
    jsr popa

    sta MVT_EGO_WIDTH       ; Save egoWidth to zero-page
    jsr popax

    sta MVT_EGO_VIEW_TAB    ; Save egoViewTab pointer
    stx MVT_EGO_VIEW_TAB + 1
    jsr popax

    sta MVT_LOCAL_VIEW_TAB  ; Save localViewTab pointer
    stx MVT_LOCAL_VIEW_TAB + 1

    ; Compute ego's center X coordinate
    GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_EGO_VIEW_TAB, sreg
    lda MVT_EGO_WIDTH
    lsr                      ; Divide width by 2
    clc                      ; Clear carry for addition
    adc sreg                 ; Add X coordinate
    sta MVT_ECX

    ; Compute local's center X coordinate
    GET_STRUCT_8_STORED_OFFSET _offsetOfXPos, MVT_LOCAL_VIEW_TAB, sreg
    lda MVT_LOCAL_CEL_WIDTH
    lsr
    clc
    adc sreg
    sta MVT_LCX

    ; Compute delta (motion threshold)
    GET_STRUCT_8_STORED_OFFSET _offsetOfParam1, MVT_LOCAL_VIEW_TAB, MVT_DELTA

    ; Calculate X direction difference and movement
    lda MVT_ECX
    sta sreg
    lda MVT_LCX
    ldy MVT_DELTA
    jsr bADirectionIndex
    sta MVT_DIFF_X
    stx MVT_DIR_VAL_X

    ; Calculate Y direction difference and movement
    GET_STRUCT_8_STORED_OFFSET _offsetOfYPos, MVT_EGO_VIEW_TAB, sreg
    GET_STRUCT_8_STORED_OFFSET _offsetOfYPos, MVT_LOCAL_VIEW_TAB
    ldy MVT_DELTA
    jsr bADirectionIndex
    sta MVT_DIFF_Y
    stx MVT_DIR_VAL_Y

    ; Map direction using the table
    txa
    READ_DIRECTION_TABLE
    sta MVT_DIR

@checkCollision: ;Do the local and ego collide?
    beq @collision

@checkIsFirstTime: ;Is this the first time we called this function since the follow ego motion started
    GET_STRUCT_8_STORED_OFFSET _offsetOfParam3, MVT_LOCAL_VIEW_TAB
    cmp #$FF ;(-1)
    beq @firstTime
@checkStopped:
    ; Check if the local view object is marked as stopped.
    GET_STRUCT_8_STORED_OFFSET _offsetOfStopped, MVT_LOCAL_VIEW_TAB
    bne @stopped            ; If stopped, branch to stopped handling logic.

@checkNonZeroMotionParam:
    ; Check if Param3 (motion-related parameter) is non-zero.
    GET_STRUCT_8_STORED_OFFSET _offsetOfParam3, MVT_LOCAL_VIEW_TAB
    bne @nonZeroMotionParam ; If non-zero, handle the motion logic.

@updateDirection:
    ; Update the local object's direction with the calculated direction value.
    lda MVT_DIR
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfDirection, MVT_LOCAL_VIEW_TAB

@end:
    ; End of subroutine, return to caller.
    rts

@collision:
    ; Handle collision scenarios.
    lda #$0                                ; Reset direction to 0.
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfDirection, MVT_LOCAL_VIEW_TAB
    lda #MOTION_NORMAL                     ; Set motion to normal.
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfMotion, MVT_LOCAL_VIEW_TAB

    ; Set flag to indicate completion of motion
    GET_STRUCT_8_STORED_OFFSET _offsetOfParam2, MVT_LOCAL_VIEW_TAB
    SET_FLAG_NON_INTERPRETER sreg
    bra @end                               ; Branch to the end of the subroutine.

@firstTime:
    ; Handle first-time movement initialization.
    lda #$0                                ; Reset Param3 to 0.
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfParam3, MVT_LOCAL_VIEW_TAB
    bra @checkNonZeroMotionParam           ; Branch to motion parameter check.

@stopped:
    ; Handle cases where the local object is stopped. 
    lda #MAX_DIRECTION + 1                 ; Generate a random direction.
    jsr _rand8Bit
    sta MVT_DIR                            ; Save the new direction.

    ; Calculate combined X and Y differences, add one, and save to X register. This is an average calculated to determine the number of steps the sprite should move in a random direction
    clc
    lda MVT_DIFF_X
    adc MVT_DIFF_Y
    lsr                                    ; Divide by 2 (logical shift right).
    inc                                    ; Increment the result.
    tax                                    ; Save result in X register.

    ; Compare the combined distance with the step size.
    GET_STRUCT_8_STORED_OFFSET _offsetOfStepSize, MVT_LOCAL_VIEW_TAB, sreg
    cpx sreg
    bcc @storeParam3                       ; Branch if distance is less than step size.

@distBiggerThanStepSize:
    ; If distance exceeds the step size, generate a random motion parameter.
    jsr randBetweenAsmCall

@storeParam3:
    ; Store the calculated motion parameter in Param3.
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfParam3, MVT_LOCAL_VIEW_TAB
    bra @updateDirection                   ; Branch to update the direction.

@nonZeroMotionParam:
    ; Handle cases where motion parameter is non-zero.
    tax                                    ; Transfer A to X.
    GET_STRUCT_8_STORED_OFFSET _offsetOfStepSize, MVT_LOCAL_VIEW_TAB, sreg

    txa                                    ; Transfer X back to A.
    sec                                    ; Set carry for subtraction.
    sbc sreg                               ; Subtract step size from motion parameter.
    bvc @storeMotionParam3                 ; If no overflow, branch to store motion parameter.
    lda #$0                                ; Otherwise, reset A to 0.

@storeMotionParam3:
    ; Store the calculated or reset motion parameter in Param3.
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfParam3, MVT_LOCAL_VIEW_TAB
    bra @end                               ; Branch to end of subroutine.


.endscope

;------------------------------------------------------------------------------
; bAWander
; Purpose:
;   Simulates wandering behavior for a local object.
; Inputs:
;   localViewTab: Pointer to the local view table
;   entryNum: Entry number of the local object
; Description:
;   If Param1 is zero, a new direction and wander distance are chosen.
;   If stopped, no movement occurs. Remaining wander steps decrement.
;------------------------------------------------------------------------------
_bAWander:
.scope
    ; Define zero-page temporary variables for internal use
    LOCAL_VIEW_TAB = ZP_TMP_2       ; Pointer to the local view table
    ENTRY_NUM = ZP_TMP_3            ; Entry number of the local object

    ; Define constants for wander distance range
    MIN_WANDER_DISTANCE = 6         ; Minimum wander distance
    MAX_WANDER_DISTANCE = 50        ; Maximum wander distance

    ; Store entryNum in ENTRY_NUM and pop the stack to retrieve parameters
    sta ENTRY_NUM                   ; Save the entryNum parameter to ENTRY_NUM
    jsr popax                       ; Pop the next value (localViewTab pointer) into A and X
    sta LOCAL_VIEW_TAB              ; Save the lower byte of localViewTab pointer
    stx LOCAL_VIEW_TAB + 1          ; Save the upper byte of localViewTab pointer

    ; Check the value of Param1 to decide on the next action
    GET_STRUCT_8_STORED_OFFSET _offsetOfParam1, LOCAL_VIEW_TAB ; Load Param1
    beq @selectDirection            ; If Param1 is zero, branch to select a new direction

    ; Decrement Param1 if it is not zero
    dec                             ; Decrease the wander step count
    sta (LOCAL_VIEW_TAB),y          ; Store the decremented value back to Param1

    ; Check if the local object is marked as stopped
    GET_STRUCT_8_STORED_OFFSET _offsetOfStopped, LOCAL_VIEW_TAB ; Load the stopped flag
    beq @end                        ; If the object is stopped, skip further logic and end

@selectDirection:
    ; Select a random direction for the object
    lda #MAX_DIRECTION + 1          ; Load the maximum number of directions (+1 for randomness)
    jsr _rand8Bit                   ; Call a random number generator for an 8-bit value
    tax                             ; Store the random direction value in the X register
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfDirection, LOCAL_VIEW_TAB ; Save direction to object

    ; Check if the entryNum is non-zero for additional behavior
    lda ENTRY_NUM                   ; Load the entryNum
    bne @setMotionParam1            ; If not zero, branch to set the motion parameters

    ; If entryNum is zero, set the direction to a predefined "ego direction"
    lda #EGODIR                     ; Load the predefined ego direction constant
    SET_VAR_NON_INTERPRETER sreg    ; Set the ego direction value for processing

@setMotionParam1:
    ; Choose a random wander distance between the defined minimum and maximum values
    lda #MIN_WANDER_DISTANCE        ; Load the minimum wander distance
    ldx #MAX_WANDER_DISTANCE        ; Load the maximum wander distance
    jsr randBetweenAsmCall          ; Generate a random value between min and max
    SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfParam1, LOCAL_VIEW_TAB ; Save the wander distance to Param1

@end:
    ; End the subroutine and return
    rts
.endscope


.endif
