.include "x16.inc"

.ifndef  VIEW_INC
VIEW_INC = 1

.import _offsetOfFlags
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfXSize
.import _offsetOfYSize
.import _offsetOfPrevY
.import _offsetOfPriority
.import _offsetOfStepTimeCount
.import _offsetOfStepTime
.import _offsetOfStepSize
.import _offsetOfRepositioned
.import _offsetOfCurrentCel
.import _viewtab
.import _sizeOfViewTab
.import _b9PreComputedPriority
.import _horizon
.import _controlMode
.import _offsetOfNumberOfLoopsVT
.import _offsetOfCurrentLoop
.import _offsetOfNumberOfCelsVT
.import _offsetOfCycleTimeCount
.import _offsetOfCycleTime
.import _offsetOfNoAdvance
.import _offsetOfCycleStatus

VIEW_POS_LOCAL_VIEW_TAB = ZP_TMP_2
VIEW_POS_ENTRY_NUM = ZP_TMP_3 + 1
VIEW_POS_LOCAL_VIEW_FLAGS = ZP_TMP_4
VIEW_POS_OTHER_VIEW_TAB = ZP_TMP_5
VIEW_POS_VIEW_POS_OTHER_VIEW_TAB = ZP_TMP_6
VIEW_POS_CAN_BE_HERE = ZP_TMP_7
VIEW_POS_ENTIRELY_ON_WATER = ZP_TMP_7 + 1
VIEW_POS_HIT_SPECIAL = ZP_TMP_8
VIEW_POS_WIDTH = ZP_TMP_8 + 1
VIEW_POS_FLAGS_LOW = ZP_TMP_9
VIEW_POS_COMPLETION_FLAG = ZP_TMP_9 + 1
VIEW_POS_STEP_SIZE = ZP_TMP_10
VIEW_POS_XPOS = ZP_TMP_10 + 1
VIEW_POS_YPOS = ZP_TMP_12
VIEW_POS_CEL_NUM = ZP_TMP_12 + 1
VIEW_POS_LOOP_NUM = ZP_TMP_13
VIEW_POS_NEW_LOOP = ZP_TMP_13 + 1
VIEW_POS_THE_CEL = ZP_TMP_14 + 1
VIEW_POS_LAST_CEL = ZP_TMP_16
VIEW_POS_ANIMATED_OBJECTS_COUNTER = ZP_TMP_16 + 1




;Don't put anything in 25 used for x and y of canBeHere, or 21 - 24, used for local variables in update position

.segment "BANKRAM09"
;boolean b9Collide(ViewTable* localViewtab, byte entryNum) 
_b9Collide:
sta VIEW_POS_ENTRY_NUM
jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1

b9CollideAsm:
ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_LOCAL_VIEW_FLAGS
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_LOCAL_VIEW_FLAGS + 1
and #>IGNOREOBJECTS
beq @loadInitialOtherViewtab
jmp @returnFalse

@loadInitialOtherViewtab:
lda #<_viewtab
sta VIEW_POS_OTHER_VIEW_TAB
lda #>_viewtab
sta VIEW_POS_OTHER_VIEW_TAB + 1

stz VIEW_POS_VIEW_POS_OTHER_VIEW_TAB

@viewTabLoop:
ldy _offsetOfFlags
lda (VIEW_POS_OTHER_VIEW_TAB),y
tax

and #ANIMATED
beq @viewTabCheck

txa
and #DRAWN
beq @viewTabCheck

iny
lda (VIEW_POS_OTHER_VIEW_TAB),y
and #>IGNOREOBJECTS
bne @viewTabCheck

lda VIEW_POS_ENTRY_NUM
cmp VIEW_POS_VIEW_POS_OTHER_VIEW_TAB
beq @viewTabCheck

ldy _offsetOfXPos ;We are going with the assumption that not colliding is less common than colliding, therefore it makes more sense to not take extra cycles by store the results for the next calculation
lda (VIEW_POS_LOCAL_VIEW_TAB),y
clc
ldy _offsetOfXSize
adc (VIEW_POS_LOCAL_VIEW_TAB),y
ldy _offsetOfXPos
cmp (VIEW_POS_OTHER_VIEW_TAB),y
bcc @returnFalse

clc
lda (VIEW_POS_OTHER_VIEW_TAB),y
ldy _offsetOfXSize
adc (VIEW_POS_OTHER_VIEW_TAB),y
ldy _offsetOfXPos
cmp (VIEW_POS_LOCAL_VIEW_TAB),y
beq @viewTabCheck
bcc @viewTabCheck

ldy _offsetOfYPos
lda (VIEW_POS_OTHER_VIEW_TAB),y
cmp (VIEW_POS_LOCAL_VIEW_TAB),y
beq @returnTrue
bcs @localYLess

;For these two we have already checked this: localViewtab->yPos > otherViewTab.yPos and localViewtab->yPos < otherViewTab.yPos 
@localYGreater:
ldy _offsetOfPrevY
lda (VIEW_POS_LOCAL_VIEW_TAB),y
cmp (VIEW_POS_OTHER_VIEW_TAB),y
bcs @returnFalse
lda #$1
rts

@localYLess:
ldy _offsetOfPrevY
lda (VIEW_POS_LOCAL_VIEW_TAB),y
cmp (VIEW_POS_OTHER_VIEW_TAB),y
beq @returnFalse
bcc @returnFalse

@returnTrue:
lda #$1
rts

@checkY:


@viewTabCheck:
clc
lda _sizeOfViewTab
adc VIEW_POS_OTHER_VIEW_TAB
sta VIEW_POS_OTHER_VIEW_TAB
lda #$0
adc VIEW_POS_OTHER_VIEW_TAB + 1
sta VIEW_POS_OTHER_VIEW_TAB + 1

inc VIEW_POS_VIEW_POS_OTHER_VIEW_TAB
lda VIEW_POS_VIEW_POS_OTHER_VIEW_TAB
cmp #VIEW_TABLE_SIZE
bcs @return
jmp @viewTabLoop

@return:

@returnFalse:
lda #$0
rts


; ---------------------------------------------------------------------------
; _b9CanBeHere
;
; Purpose:
;   Determines whether a view/object can be placed at its current position.
;   Implements the same logic as the C# method `CanBeHere()`:
;     - Checks if priority is fixed; if not, calculates it based on Y position.
;     - If priority == HIGHEST_PRIORITY (15), skips all control-line testing.
;     - Otherwise scans each pixel along the object's baseline:
;         * Priority 3 = water → contributes to "entirely on water".
;         * Priority 0 = hard block → cannot be here.
;         * Priority 1 = block unless IGNOREBLOCKS flag set.
;         * Priority 2 = marks "hitSpecial".
;     - After scanning:
;         * If entirely on water but "StayOnLand" → cannot be here.
;         * If not entirely on water but "StayOnWater" → cannot be here.
;     - If this is the ego object (entry #0), sets global ONWATER/HITSPEC flags.
;   Returns:
;     - A (accumulator) = 1 if object can be here, 0 if not.
;
; Key scratch variables:
;   X_VAL / Y_VAL = running baseline pixel position
;   VIEW_POS_CAN_BE_HERE      = boolean (default true, cleared if blocked)
;   VIEW_POS_ENTIRELY_ON_WATER= boolean (default false, set true then cleared)
;   VIEW_POS_HIT_SPECIAL      = boolean (set if special pixel seen)
;
; ---------------------------------------------------------------------------
;boolean b9CanBeHere(ViewTable* localViewtab, byte entryNum);
_b9CanBeHere: ;Call this from C, call b9CanBeHereFromAsm
sta VIEW_POS_ENTRY_NUM            ; A = object/entry index (ego == 0)

jsr popax                         ; pop pointer to view table into A/X
sta VIEW_POS_LOCAL_VIEW_TAB       ;   low byte of pointer
stx VIEW_POS_LOCAL_VIEW_TAB + 1   ;   high byte

b9CanBeHereAsm:
.scope
X_VAL = ZP_TMP_25                 ; scratch: current X being scanned along baseline
Y_VAL = ZP_TMP_25 + 1             ; scratch: Y of the baseline

lda #$1
sta VIEW_POS_CAN_BE_HERE          ; canBeHere = true
stz VIEW_POS_ENTIRELY_ON_WATER    ; entirelyOnWater = false
stz VIEW_POS_HIT_SPECIAL          ; hitSpecial = false

; --- Check FixedPriority flag ---
ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_FLAGS_LOW
and #FIXEDPRIORITY
bne @checkIfHighestPriority        ; skip recompute if fixed priority

; --- Calculate priority from Y (CalculatePriority) ---
ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
tay
lda _b9PreComputedPriority,y
ldy _offsetOfPriority
sta (VIEW_POS_LOCAL_VIEW_TAB),y

@checkIfHighestPriority: ; if priority == HIGHEST_PRIORITY, skip baseline check
ldy _offsetOfPriority
lda (VIEW_POS_LOCAL_VIEW_TAB),y
cmp #HIGHEST_PRIORITY
bne @calculateIfCanBeHere
jmp @setEgoWaterSpecialFlags

@calculateIfCanBeHere: ; setup for baseline scan loop
inc VIEW_POS_ENTIRELY_ON_WATER    ; assume entirely on water

ldy _offsetOfXPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta X_VAL                         ; start X

ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta Y_VAL                         ; Y coordinate of baseline

ldy _offsetOfXSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y
clc
adc X_VAL
sta VIEW_POS_WIDTH                ; end X
lda X_VAL
bra @postIncrement

@checkAgainstControlLoopCheck: ; loop body increment
lda X_VAL
inc

@postIncrement: ; check loop exit condition
cmp VIEW_POS_WIDTH
beq @endCheckAgainstControlLoop
sta X_VAL

GET_PRIORITY                      ; A = priority at (X_VAL,Y_VAL)

; --- Priority == WATER (3) ---
cmp #WATER
beq @checkAgainstControlLoopCheck ; still water, continue
stz VIEW_POS_ENTIRELY_ON_WATER    ; saw non-water → entirelyOnWater=false

@checkPriority0: ; if priority == 0 (hard block)
cmp #$0
bne @checkPriority1
stz VIEW_POS_CAN_BE_HERE
bra @checkAgainstControlLoopCheck

@checkPriority1: ; if priority == 1 (soft block, unless IGNOREBLOCKS)
cmp #$1
bne @checkPriority2
tax
lda VIEW_POS_FLAGS_LOW
and #IGNOREBLOCKS
bne @checkAgainstControlLoopCheck ; ignore blocks → continue
stz VIEW_POS_CAN_BE_HERE          ; else blocked
bra @endCheckAgainstControlLoop   ; break

@checkPriority2: ; if priority == 2 (special hit)
cmp #2
bne @checkAgainstControlLoopCheck
lda #$1
sta VIEW_POS_HIT_SPECIAL
bra @checkAgainstControlLoopCheck

@endCheckAgainstControlLoop: ; loop finished

@checkEntirelyOnWater: ; post-check: entirely on water case
lda VIEW_POS_ENTIRELY_ON_WATER
beq @checkHitSpecial
ldy _offsetOfFlags
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #>ONLAND
beq @setEgoWaterSpecialFlags
stz VIEW_POS_CAN_BE_HERE          ; StayOnLand but entirely on water → fail
bra @setEgoWaterSpecialFlags

@checkHitSpecial: ; post-check: not entirely on water case
ldy _offsetOfFlags
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #>ONWATER
beq @setEgoWaterSpecialFlags
stz VIEW_POS_CAN_BE_HERE          ; StayOnWater but not entirely on water → fail

@setEgoWaterSpecialFlags: ; ego-specific flag setting
lda VIEW_POS_ENTRY_NUM
bne @return                       ; skip if not ego

@setOnWaterFlag:
lda VIEW_POS_ENTIRELY_ON_WATER
beq @setOnHitSpecialFlag
lda #FLAG_ON_WATER
SET_FLAG_NON_INTERPRETER sreg

@setOnHitSpecialFlag:
lda VIEW_POS_HIT_SPECIAL
beq @return
lda #FLAG_HIT_SPECIAL
SET_FLAG_NON_INTERPRETER sreg

@return: ; return value
lda VIEW_POS_CAN_BE_HERE
rts
.endscope


; -----------------------------------------------------------------------------
; b9GoodPositionAsm
; 
; Purpose:
;   Validate that a VIEW's (X,Y) and size (XSize,YSize) lie within screen
;   bounds and horizon rules, mirroring:
;     return ( X >= MINX
;              && (X + XSize) <= MAXX + 1
;              && (Y - YSize) >= MINY - 1
;              && Y <= MAXY
;              && (IgnoreHorizon || Y > Horizon) );
;
; Contract:
;   - Returns: A = $01 (TRUE) if valid, A = $00 (FALSE) otherwise. RTS.
;   - Clobbers: A, X, Y, and flags (NZVC).
;   - Reads from: VIEW_POS_LOCAL_VIEW_TAB + offsets (X, XSize, Y, YSize, Flags)
;   - Constants:
;       PICTURE_WIDTH,  PICTURE_HEIGHT
;       IGNOREHORIZON bit mask
;   - Horizon:
;       Separate check: pass if IgnoreHorizon set; otherwise require Y > Horizon.
;
; 6502 nuance:
;   - CMP is unsigned. To approximate signed lower-bound checks (e.g. allow -1),
;     code uses sentinel ranges:
;       * For X lower bound: treat values >= PICTURE_WIDTH+64 as "wrapped negatives"
;         and reject (heuristic since 6502 can't distinguish negatives directly).
;       * For (Y - YSize) >= -1: explicit compare to $FF (i.e., -1) as a pass.
;   - C (carry) must be set before SBC; BEQ/BCS/BCC follow unsigned semantics.
; -----------------------------------------------------------------------------
;void b9FindPosition(Viewtab* localViewTab, byte entryNum);
b9GoodPositionAsm:
    ; ---- C0: X >= MINX (with "wrapped negative" guard) -----------------------
    ; Load X
    ldy _offsetOfXPos                    ; Y := offset of X in Viewtab
    lda (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := X

    ; Heuristic: if A >= (PICTURE_WIDTH + 64), assume it's an invalid "negative"
    ; (i.e., underflow wrap interpreted as a large unsigned). Reject.
    cmp #PICTURE_WIDTH + 64              ; X >= WIDTH+64 ? -> reject
    bcs @returnFalse

    ; ---- C1: (X + XSize) <= PICTURE_WIDTH -----------------------------------
    ; Note: C# uses (X + XSize) <= MAXX + 1 to allow right edge inclusive.
    ; Here, compare to PICTURE_WIDTH (same effect).
    ldy _offsetOfXSize                   ; Y := offset of XSize
    adc (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := X + XSize  (adc uses C from prior cmp: carry unaffected by CMP)
                                         ; (CMP does not change C to a meaningful add carry; but adding size
                                         ;  after comparing is fine because we only rely on A result for CMP below)

    cmp #PICTURE_WIDTH                   ; (X + XSize) ? PICTURE_WIDTH
    beq @checkY                          ; equal is OK
    bcs @returnFalse                     ; greater -> reject

@checkY:
    ; ---- Prep Y and compute (Y - YSize) -------------------------------------
    ; Preserve Y (position) in X for later upper-bound/horizon checks.
    ldy _offsetOfYPos                    ; Y := offset of Y
    lda (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := Y
    tax                                  ; X := Y (save)

    ; Compute Y - YSize with borrow
    sec                                  ; set carry for SBC (no borrow)
    ldy _offsetOfYSize                   ; Y := offset of YSize
    sbc (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := Y - YSize

    ; ---- C2: (Y - YSize) >= -1  ---------------------------------------------
    ; Allow -1 exactly (C# uses MINY - 1).
    cmp #$FF                             ; A == $FF (-1) ? -> pass to next check
    beq @checkLessThanMaxY

    ; As with X low bound, reject values that look like wrapped negatives:
    ; if (Y - YSize) >= PICTURE_HEIGHT + 64, treat as invalid "negative".
    cmp #PICTURE_HEIGHT + 64
    bcs @returnFalse                     ; outside allowed low-bound heuristic -> reject

@checkLessThanMaxY:
    ; ---- C3: Y <= PICTURE_HEIGHT  -------------------------------------------
    txa                                  ; restore Y into A
    cmp #PICTURE_HEIGHT                  ; Y ? PICTURE_HEIGHT
    beq @checkHorizon                    ; equal is OK
    bcs @returnFalse                     ; greater -> reject

@checkHorizon:
    ; ---- C4: IgnoreHorizon || (Y > Horizon)  --------------------------------
    ; If IgnoreHorizon flag set, pass immediately.
    ldy _offsetOfFlags                   ; Y := offset of flags
    lda (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := flags
    and #IGNOREHORIZON                   ; test IgnoreHorizon bit
    bne @returnTrue                      ; set -> pass

    ; Otherwise require Y > Horizon (strictly greater).
    ; Load Y again and compare to _horizon:
    ldy _offsetOfYPos                    ; Y := offset of Y (comment kept aligned with code)
    lda (VIEW_POS_LOCAL_VIEW_TAB),y      ; A := Y
    cmp _horizon                         ; Y ? Horizon
    beq @returnFalse                     ; Y == Horizon -> fail (not strictly greater)
    bcc @returnFalse                     ; Y < Horizon  -> fail

@returnTrue:
    lda #$1                              ; TRUE
    rts

@returnFalse:
    lda #$0                              ; FALSE
    rts

; -----------------------------------------------------------------------------
; _b9FindPosition
;
; Purpose:
;   Attempt to place a VIEW object at a valid (X,Y) coordinate in the picture.
;   Mirrors C# FindPosition():
;     - Adjust Y below horizon if needed.
;     - If current position is good (no collision, can be here), return.
;     - Otherwise, scan outward in a spiral pattern (left, down, right, up),
;       expanding the leg length as needed until a valid position is found.
;
; Inputs:
;   - A: entryNum
;   - Stack: pointer to Viewtab (popax restores it into A/X)
;   - Globals: _horizon, VIEW_POS_LOCAL_VIEW_TAB, VIEW_POS_ENTRY_NUM
;   - Offsets: _offsetOfFlags, _offsetOfXPos, _offsetOfYPos
;   - Functions called:
;       b9GoodPositionAsm (geometry bounds check)
;       b9CollideAsm      (collision check)
;       b9CanBeHereAsm      (environment rule check)
;
; Outputs:
;   - Adjusts Viewtab’s X/Y in place until constraints satisfied.
;   - Returns: RTS (no explicit value).
;
; Registers clobbered:
;   A, X, Y
;
; Locals (ZP scratch):
;   LEG_LEN = ZP_TMP_9 + 1   ; spiral leg length
;   LEG_DIR = ZP_TMP_10      ; current leg direction (0=left,1=down,2=right,3=up)
;   LEG_CNT = ZP_TMP_10 + 1  ; remaining steps in this leg
; -----------------------------------------------------------------------------
; extern boolean b9FindPosition(ViewTable* localViewTab, byte entryNum);

_b9FindPosition:
LEG_LEN = ZP_TMP_9 + 1
LEG_DIR = ZP_TMP_10
LEG_CNT = ZP_TMP_10 + 1

    ; Save entry number and local view pointer
    sta VIEW_POS_ENTRY_NUM
    jsr popax
    sta VIEW_POS_LOCAL_VIEW_TAB         ; pointer low byte
    stx VIEW_POS_LOCAL_VIEW_TAB + 1     ; pointer high byte

_b9FindPositionAsm:
@checkIgnoreHorizon:
    ; If IgnoreHorizon is set, skip horizon adjustment
    ldy _offsetOfFlags
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    and #IGNOREHORIZON
    bne @checkAlreadyGoodPosition

    ; Otherwise, ensure Y > horizon
    lda _horizon
    tax                                  ; save horizon in X
    ldy _offsetOfYPos
    cmp (VIEW_POS_LOCAL_VIEW_TAB),y      ; compare horizon to Y

    beq @setYToHorizon                   ; if Y == horizon, bump it
    bcc @checkAlreadyGoodPosition        ; if Y < horizon, already OK

@setYToHorizon:
    inx                                  ; horizon + 1
    txa
    sta (VIEW_POS_LOCAL_VIEW_TAB),y      ; Y := horizon+1

@checkAlreadyGoodPosition:
    ; If position is already valid (bounds, no collision, allowed), return
    jsr b9GoodPositionAsm
    beq @findGoodPositionLoopInit
    jsr b9CollideAsm
    bne @findGoodPositionLoopInit
    jsr b9CanBeHereAsm
    beq @findGoodPositionLoopInit

@alreadyGoodPosition:
    rts

@findGoodPositionLoopInit:
    ; Initialize spiral search:
    ; legLen=1, legDir=0 (left), legCnt=1
    lda #$1
    sta LEG_LEN
    stz LEG_DIR
    sta LEG_CNT

@findGoodPositionLoop:
    ; Check if current spot is valid
    jsr b9GoodPositionAsm
    beq @findGoodPositionLoopBody
    jsr b9CollideAsm
    bne @findGoodPositionLoopBody
    jsr b9CanBeHereAsm
    beq @findGoodPositionLoopBody

    ; Success → return
    bra @return

@findGoodPositionLoopBody:
    ; Spiral movement depending on LEG_DIR
    lda LEG_DIR

@case0:
    bne @case1
    ; --- Case 0: Move left (X--) ---
    ldy _offsetOfXPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    dec
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    dec LEG_CNT
    bne @findGoodPositionLoop            ; continue leg

    lda #$1
    sta LEG_DIR                          ; next direction: down
    lda LEG_LEN
    sta LEG_CNT
    bra @findGoodPositionLoop

@case1:
    cmp #$1
    bne @case2
    ; --- Case 1: Move down (Y++) ---
    ldy _offsetOfYPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    inc
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    dec LEG_CNT
    bne @findGoodPositionLoop

    lda #$2
    sta LEG_DIR                          ; next direction: right
    lda LEG_LEN
    inc
    sta LEG_LEN                          ; increase leg length
    sta LEG_CNT
    bne @findGoodPositionLoop

@case2:
    cmp #$2
    bne @case3
    ; --- Case 2: Move right (X++) ---
    ldy _offsetOfXPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    inc
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    dec LEG_CNT
    bne @findGoodPositionLoop

    lda #$3
    sta LEG_DIR                          ; next direction: up
    lda LEG_LEN
    sta LEG_CNT
    bne @findGoodPositionLoop

@case3:
    ; --- Case 3: Move up (Y--) ---
    ldy _offsetOfYPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    dec
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    dec LEG_CNT
    bne @findGoodPositionLoop

    stz LEG_DIR                          ; wrap to direction 0 (left)
    lda LEG_LEN
    inc
    sta LEG_LEN                          ; increase leg length
    sta LEG_CNT
    bra @findGoodPositionLoop

@return:
    rts

;void b9StartMoveObj(ViewTable* localViewTab, byte entryNum, byte x, byte y, byte stepSize, byte completionFlag)
b9StartMoveObj:
 ; Save entry number and local view pointer
sta VIEW_POS_COMPLETION_FLAG

jsr popa 
sta VIEW_POS_STEP_SIZE

jsr popax
stx VIEW_POS_YPOS
sta VIEW_POS_XPOS

jsr popa
sta VIEW_POS_ENTRY_NUM
jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB         ; pointer low byte
stx VIEW_POS_LOCAL_VIEW_TAB + 1     ; pointer high byte

b9StartMoveObjAsm:

ldy _offsetOfMotion
lda  MOTION_MOVETO
sta (VIEW_POS_LOCAL_VIEW_TAB),y

ldy _offsetOfYPos
lda VIEW_POS_YPOS
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

ldy _offsetOfStepSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y 
ldy _offsetOfParam3
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

lda VIEW_POS_STEP_SIZE
beq @setMotionParam4

@nonZeroStepSize:
ldy _offsetOfStepSize
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

@setMotionParam4:
lda VIEW_POS_COMPLETION_FLAG
ldy _offsetOfParam4
sta (VIEW_POS_LOCAL_VIEW_TAB),y 
ldx #$0
SET_VAR_NON_INTERPRETER sreg

lda VIEW_POS_FLAGS_LOW
ldy _offsetOfParam4
lda (VIEW_POS_LOCAL_VIEW_TAB),y 
ora #UPDATE
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

@checkifEgo:
lda VIEW_POS_ENTRY_NUM
bne @moveTo

lda #PLAYER_CONTROL
sta _controlMode
bne @moveTo

@moveTo:

lda VIEW_POS_LOCAL_VIEW_TAB
ldx VIEW_POS_LOCAL_VIEW_TAB + 1
jsr pushax
lda VIEW_POS_ENTRY_NUM
TRAMPOLINE #MOVEMENT_BANK, bAMoveTo

rts

b9EndMoveObj:

ldy _offsetOfParam3
lda (VIEW_POS_LOCAL_VIEW_TAB),y

ldy _offsetOfStepSize
sta (VIEW_POS_LOCAL_VIEW_TAB),y

ldy _offsetOfParam4
lda (VIEW_POS_LOCAL_VIEW_TAB),y
SET_FLAG_NON_INTERPRETER sreg

lda VIEW_POS_ENTRY_NUM
bne @end

lda #PROGRAM_CONTROL
sta _controlMode

lda #EGODIR
ldx #$0
SET_VAR_NON_INTERPRETER sreg

@end:
rts

VIEW_TOP_BORDER = 1
VIEW_RIGHT_BORDER = 2
VIEW_BOTTOM_BORDER = 3
VIEW_LEFT_BORDER = 4

xs_tab:
    .byte 0, 0, 1, 1, 1, 0, $FF, $FF, $FF
ys_tab:
    .byte 0, $FF, $FF, 0, 1, 1, 1, 0, $FF

_b9UpdatePosition:
sta VIEW_POS_ENTRY_NUM
jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB         ; pointer low byte
stx VIEW_POS_LOCAL_VIEW_TAB + 1     ; pointer high byte


b9UpdatePositionAsm:
.scope
BORDER = ZP_TMP_21
OX = ZP_TMP_21 + 1
PX = ZP_TMP_22
OY = ZP_TMP_22 + 1
PY = ZP_TMP_23
OD = ZP_TMP_24 
OS = ZP_TMP_24 + 1

sta VIEW_POS_ENTRY_NUM
jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1

ldy _offsetOfStepTimeCount
lda (VIEW_POS_LOCAL_VIEW_TAB),y

beq @resetStepTimeClock

dec
sta (VIEW_POS_LOCAL_VIEW_TAB),y
beq @resetStepTimeClock
rts


@resetStepTimeClock:
ldy _offsetOfStepTime
lda (VIEW_POS_LOCAL_VIEW_TAB),y
ldy _offsetOfStepTimeCount
sta (VIEW_POS_LOCAL_VIEW_TAB),y

stz BORDER


sta OX
sta PX

ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta OY
sta PY
tax ;Used in stepOperatorLocationY careful about using X

ldy _offsetOfRepositioned
lda (VIEW_POS_LOCAL_VIEW_TAB),y
bne @checkBorders

ldy _offsetOfStepSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta OS

ldy _offsetOfDirection
lda (VIEW_POS_LOCAL_VIEW_TAB),y
tax

@determineMovementX:
lda xs_tab,x
beq @determineMovementY
bmi @subX

@addX:
clc
lda OX
adc OS
sta OX

bra @determineMovementY

@subX:
sec
lda OX
sbc OS
sta OX

@determineMovementY:
lda ys_tab,x

beq @checkBorders
bmi @subY

@addY:
clc
lda OY
adc OS
sta OY
bra @checkBorders

@subY:
sec
lda OY
sbc OS
sta OY

@checkBorders:
lda OX
@checkLessThanXMin:
cmp #PICTURE_WIDTH + 64
bcc @checkGreaterThanXMax
@setMinLeft:
stz OX
lda #VIEW_LEFT_BORDER
sta BORDER

@checkGreaterThanXMax:
ldy _offsetOfXSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta sreg
clc
lda OX
adc sreg
cmp #PICTURE_WIDTH + 1
bcc @checkLessThanYMin

lda #PICTURE_WIDTH
sec
sbc sreg
sta OX
lda #VIEW_RIGHT_BORDER
sta BORDER

@checkLessThanYMin:
ldy _offsetOfYSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta sreg

sec
lda OY
sbc sreg

cmp #PICTURE_HEIGHT + 64
bcc @checkGreaterThanYMax
cmp #$FF
beq @checkGreaterThanYMax

lda #$FF
clc
adc sreg
sta OY
lda #VIEW_TOP_BORDER
sta BORDER
bra @updatePosition

@checkGreaterThanYMax:
lda OY
cmp #PICTURE_HEIGHT + 1
bcc @checkLessThanHorizon

lda #PICTURE_HEIGHT - 1
sta OY
lda #VIEW_BOTTOM_BORDER
sta BORDER
bra @updatePosition

@checkLessThanHorizon:
ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #IGNOREHORIZON
bne @updatePosition

lda _horizon
cmp OY
bcc @updatePosition

inc
sta OY

lda #VIEW_TOP_BORDER
sta BORDER

@updatePosition:

ldy _offsetOfXPos
lda OX
sta (VIEW_POS_LOCAL_VIEW_TAB),y

ldy _offsetOfYPos
lda OY
sta (VIEW_POS_LOCAL_VIEW_TAB),y

jsr b9CollideAsm
bne @callFindPosition
jsr b9CanBeHereAsm
bne @checkBorderCollision

@callFindPosition:
ldy _offsetOfXPos
lda PX
sta (VIEW_POS_LOCAL_VIEW_TAB),y

ldy _offsetOfYPos
lda PY
sta (VIEW_POS_LOCAL_VIEW_TAB),y

stz BORDER
jsr _b9FindPositionAsm

@checkBorderCollision:

lda BORDER ;Even though the C# said greater than 0, not zero is better in 6502, as there are less instructions
beq @setRepositioned

@borderHandling:
lda VIEW_POS_ENTRY_NUM
bne @nonEgoBorderHandling

@egoBorderHandling:
lda #EGOEDGE
ldx BORDER
SET_VAR_NON_INTERPRETER sreg
bra @checkMotionType

@nonEgoBorderHandling:
lda #OBJHIT
ldx VIEW_POS_ENTRY_NUM
SET_VAR_NON_INTERPRETER sreg

lda #OBJEDGE
ldx BORDER
SET_VAR_NON_INTERPRETER sreg

@checkMotionType:
ldy _offsetOfMotion
lda #MOTION_MOVETO
cmp (VIEW_POS_LOCAL_VIEW_TAB),y
bne @setRepositioned

@endMoveObj:
jsr b9EndMoveObj

@setRepositioned:

ldy _offsetOfRepositioned
lda #$0
sta (VIEW_POS_LOCAL_VIEW_TAB),y

rts
.endscope

; void b9SetCel(ViewTable* localViewTab, byte entryNum, byte celNum)
; WARNING: Non-conventional calling. Assumes arguments are pre-loaded in zero page:
; - VIEW_POS_LOCAL_VIEW_TAB: Pointer to the view table (localViewTab)
; - VIEW_POS_CEL_NUM: Cel number to set (celNum)
; - entryNum is not used in this routine
b9SetCel:
    ; Set the current cel number in the view table
    lda VIEW_POS_CEL_NUM            ; Load cel number from zero page
    ldy _offsetOfCurrentCel         ; Y = offset of CurrentCel in view table
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Store cel number in CurrentCel

    ; Validate CurrentLoop < NumberOfLoops
    ldy _offsetOfCurrentLoop        ; Y = offset of CurrentLoop
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = CurrentLoop
    ldy _offsetOfNumberOfLoopsVT    ; Y = offset of NumberOfLoops
    cmp (VIEW_POS_LOCAL_VIEW_TAB),y ; Compare CurrentLoop with NumberOfLoops
    bcs @return                     ; If CurrentLoop >= NumberOfLoops, return

    ; Validate CurrentCel < NumberOfCels
    ldy _offsetOfCurrentCel         ; Y = offset of CurrentCel
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = CurrentCel
    ldy _offsetOfNumberOfCelsVT     ; Y = offset of NumberOfCels
    cmp (VIEW_POS_LOCAL_VIEW_TAB),y ; Compare CurrentCel with NumberOfCels
    bcs @return                     ; If CurrentCel >= NumberOfCels, return

@borderCollision:
    ; Check X-axis border collision: XPos + XSize > PICTURE_WIDTH
    ldy _offsetOfXPos               ; Y = offset of XPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = XPos
    clc                             ; Clear carry for addition
    ldy _offsetOfXSize              ; Y = offset of XSize
    adc (VIEW_POS_LOCAL_VIEW_TAB),y ; A = XPos + XSize
    cmp #PICTURE_WIDTH + 64         ; Check for wraparound (overflow in 8-bit arithmetic)
    bcs @checkY                     ; If >= PICTURE_WIDTH + 64, skip to Y check (handles negative wrap)
    cmp #PICTURE_WIDTH              ; Compare with PICTURE_WIDTH
    beq @checkY                     ; If = PICTURE_WIDTH, skip to Y check
    bcc @checkY                     ; If < PICTURE_WIDTH, skip to Y check

@setRepositionedBasedOnX:
    ; Set Repositioned flag for X-axis collision
    ldy _offsetOfRepositioned       ; Y = offset of Repositioned
    lda #$1                         ; A = 1 (true)
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Set Repositioned = true

@clampToMaxX:
    ; Clamp XPos to PICTURE_WIDTH - 1 - XSize
    sec                             ; Set carry for subtraction
    lda #PICTURE_WIDTH - 1         ; A = PICTURE_WIDTH - 1 (equivalent to Defines.MAXX)
    ldy _offsetOfXSize              ; Y = offset of XSize
    sbc (VIEW_POS_LOCAL_VIEW_TAB),y ; A = PICTURE_WIDTH - 1 - XSize
    ldy _offsetOfXPos               ; Y = offset of XPos
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Set XPos = PICTURE_WIDTH - 1 - XSize

@checkY:
    ; Check Y-axis border collision: YPos - YSize < Defines.MINY - 1 (assumed 255)
    sec                             ; Set carry for subtraction
    ldy _offsetOfYPos               ; Y = offset of YPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = YPos
    ldy _offsetOfYSize              ; Y = offset of YSize
    sbc (VIEW_POS_LOCAL_VIEW_TAB),y ; A = YPos - YSize
    cmp #PICTURE_HEIGHT + 64        ; Check for underflow (high value due to 8-bit wraparound)
    bcc @return                     ; If < PICTURE_HEIGHT + 64, no collision, return

@setRepositionedBasedOnY:
    ; Set Repositioned flag for Y-axis collision
    ldy _offsetOfRepositioned       ; Y = offset of Repositioned
    lda #$1                         ; A = 1 (true)
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Set Repositioned = true

    ; Adjust YPos to Defines.MINY - 1 + YSize (assumes Defines.MINY - 1 = 255)
    clc                             ; Clear carry for addition
    lda #$FF                        ; A = 255 (Defines.MINY - 1)
    ldy _offsetOfYSize              ; Y = offset of YSize
    adc (VIEW_POS_LOCAL_VIEW_TAB),y ; A = 255 + YSize (mod 256, effectively YSize - 1)
    ldy _offsetOfYPos               ; Y = offset of YPos
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Set YPos = 255 + YSize

@clampToMaxY:
    ; Check if YPos is below or at horizon
    ldy _offsetOfYPos               ; Y = offset of YPos
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = YPos
    cmp _horizon                    ; Compare YPos with horizon
    beq @checkIgnoreHorizon         ; If YPos = horizon, check IgnoreHorizon flag
    bcs @return                     ; If YPos > horizon, return

@checkIgnoreHorizon:
    ; Check if IgnoreHorizon flag is set
    ldy _offsetOfFlags              ; Y = offset of Flags
    lda (VIEW_POS_LOCAL_VIEW_TAB),y ; A = Flags
    and #IGNOREHORIZON              ; Check IgnoreHorizon bit
    bne @return                     ; If IgnoreHorizon is set, return

@clampToHorizon:
    ; Clamp YPos to horizon + 1
    lda _horizon                    ; A = horizon
    inc                             ; A = horizon + 1
    ldy _offsetOfYPos               ; Y = offset of YPos
    sta (VIEW_POS_LOCAL_VIEW_TAB),y ; Set YPos = horizon + 1

@return:
    rts                             ; Return from subroutine

b9SetLoop:
; void b9SetLoop(ViewTable* localViewTab, byte entryNum, byte celNum)
; WARNING: Non-conventional calling. Assumes arguments are pre-loaded in zero page:
; - VIEW_POS_LOCAL_VIEW_TAB: Pointer to the view table (localViewTab)
; - VIEW_POS_LOOP_NUM: Loop number to set (loopNum)
; - entryNum is not used in this routine
lda VIEW_POS_LOOP_NUM
ldy _offsetOfCurrentLoop
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

ldy _offsetOfCurrentLoop
lda (VIEW_POS_LOCAL_VIEW_TAB),y 
ldy _offsetOfNumberOfLoopsVT
cmp (VIEW_POS_LOCAL_VIEW_TAB),y 
bcs @setCurrentCelToZero

ldy _offsetOfCurrentCel
lda (VIEW_POS_LOCAL_VIEW_TAB),y 
ldy _offsetOfNumberOfCelsVT
cmp (VIEW_POS_LOCAL_VIEW_TAB),y 
bcc @setCel

@setCurrentCelToZero:
ldy _offsetOfCurrentCel
lda #$0
sta (VIEW_POS_LOCAL_VIEW_TAB),y 

@setCel:

jsr b9SetCel
rts


; b9UpdateLoopAndCel
; ------------------
; Purpose: Updates the animation loop and cel (frame) of a view object based on its direction,
; number of loops, and timing conditions. Used in a game engine to manage sprite animations,
; ensuring correct animation sequences (e.g., walking left, right, or stopped) are displayed.
; WARNING: Non-conventional calling. Assumes arguments are pre-loaded in zero page
; Inputs:
;   - VIEW_POS_LOCAL_VIEW_TAB: Pointer to the view table containing object properties
;     (direction, number of loops, flags, step time count, cycle time count, current loop).
;   - Y register: Used to index into the view table at specific offsets.
; Outputs:
;   - Updates VIEW_POS_NEW_LOOP with the new loop number based on direction.
;   - Updates VIEW_POS_LOOP_NUM when a new loop is set.
;   - Modifies cycle time count and advances cel in the view table when appropriate.
;   - Calls b9SetLoop and b9AdvanceCel subroutines to apply changes.
; Flow:
;   1. Initializes new loop to Stopped state.
;   2. Checks for fixed loop flag; skips loop selection if set.
;   3. Determines number of loops (2, 3, or 4) and selects new loop from lookup tables (twoLoop or fourLoop).
;   4. Updates loop if step time allows and new loop differs from current loop.
;   5. If cycling, manages cycle time count and advances cel when count reaches zero.
;   6. Resets cycle time and returns.
; Notes:
;   - Likely part of a game engine (e.g., Sierra's AGI system).
;   - Skips a 'kq4 check' (possibly King's Quest IV-specific logic).
;   - Assumes b9SetLoop and b9AdvanceCel subroutines handle low-level updates.

b9UpdateLoopAndCel:
.scope
    ; Constants for movement directions and states
    S = 4  ; Stopped
    R = 0  ; Right
    L = 1  ; Left
    F = 2  ; Forward
    B = 3  ; Backward


bra @start
    ; Define the twoLoop table
    ; Maps direction to loop number for objects with 2 or 3 loops
@twoLoop:
       .byte S, S, R, R, R, S, L, L, L  ; Lookup table for loop selection

    ; Define the fourLoop table
    ; Maps direction to loop number for objects with 4 loops
@fourLoop:
        .byte S, B, R, R, R, F, L, L, L  ; Lookup table for loop selection

    ; Initialize new loop to Stopped state
@start:
    lda #S
    sta VIEW_POS_NEW_LOOP

@checkFixedLoop:
    ; Check if the object has a fixed loop flag set
    ldy _offsetOfFlags + 1
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    and #>FIXEDLOOP
    beq @checkIfLoopShouldBeSet  ; Skip to loop setting if fixed loop is set

@getNewLoopBasedOnDirection:
    ; Get the number of loops for the current view
    ldy _offsetOfNumberOfLoopsVT
    lda (VIEW_POS_LOCAL_VIEW_TAB),y

@checkForTwoOrThreeLoops:
    ; Check if the view has 2 loops
    cmp #$2
    beq @twoOrThreeLoop
    ; Check if the view has 3 loops
    cmp #$3
    bne @checkForFourLoops
@twoOrThreeLoop:
    ; For 2 or 3 loops, get the direction and map to new loop using twoLoop table
    ldy _offsetOfDirection
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    lda @twoLoop,y
    sta VIEW_POS_NEW_LOOP
    bra @checkIfLoopShouldBeSet

@checkForFourLoops:
    ; Check if the view has 4 loops
    cmp #$4
    bne @checkIfLoopShouldBeSet
    ; For 4 loops, get the direction and map to new loop using fourLoop table
    ldy _offsetOfDirection
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    tay
    lda @fourLoop,y
    sta VIEW_POS_NEW_LOOP
    ; Note: Skips kq4 check (commented as not implemented)

@checkIfLoopShouldBeSet:  
    ; Check if step time count is 1 (time to update loop)
    ldy _offsetOfStepTimeCount
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp #$1
    bne @checkIfCelShouldBeAdvanced  ; Skip if not time to update
    ; Check if new loop is Stopped
    lda VIEW_POS_NEW_LOOP
    cmp #S
    beq @checkIfCelShouldBeAdvanced  ; Skip if new loop is Stopped
    ; Compare current loop with new loop
    ldy _offsetOfCurrentLoop
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp VIEW_POS_NEW_LOOP
    beq @checkIfCelShouldBeAdvanced  ; Skip if they're the same
    ; Update the loop number and call subroutine to set it
    lda VIEW_POS_NEW_LOOP
    sta VIEW_POS_LOOP_NUM
    jsr b9SetLoop

@checkIfCelShouldBeAdvanced:
    ; Check if the object is cycling (animation active)
    ldy _offsetOfFlags
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    and #CYCLING
    beq @return  ; Exit if not cycling
 
    ; Check cycle time count
    ldy _offsetOfCycleTimeCount
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    beq @return  ; Exit if cycle time is zero
    
    @callAdvanceCel:
    ; Decrease cycle time count
    dec
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    bne @return  ; Exit if cycle time count is not zero
    ; Advance to next cel (animation frame)
    jsr b9AdvanceCel

    ; Reset cycle time count to initial cycle time
    ldy _offsetOfCycleTime
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy _offsetOfCycleTimeCount
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

@return:
    ; Return from subroutine
    rts
.endscope

; b9AdvanceCel
; -------------
; Purpose:
;   Advance the sprite's cel (frame) according to CycleType:
;     - Normal:        ++cel, wrap to 0 past last
;     - EndLoop:       advance until last cel, then set flag + stop cycling
;     - ReverseLoop:   decrement until 0, then set flag + stop cycling
;     - Reverse:       --cel, wrap to last from 0
;
; Parity:
;   This routine directly mirrors the AdvanceCel() logic from the C# version.
;
; Calling/Assumptions:
;   - VIEW_POS_LOCAL_VIEW_TAB points to the per-view state table (zero-page pointer).
;   - Offsets (_offsetOf*) index fields within that table via (VIEW_POS_LOCAL_VIEW_TAB),Y.
;   - VIEW_POS_THE_CEL is a working register for the cel index.
;   - VIEW_POS_LAST_CEL is used as a scratch register (NumberOfCels - 1).
;
; Inputs:
;   - NoAdvance: non-zero → skip advance for this frame, then clear.
;   - CurrentCel: current cel index.
;   - NumberOfCels: total cel count.
;   - CycleType: 0=Normal, 1=EndLoop, 2=ReverseLoop, 3=Reverse.
;   - Flags: bitfield containing CYCLING.
;   - Param1: flag index for EndLoop/ReverseLoop finish signaling.
;
; Outputs/Side Effects:
;   - Advances cel per CycleType with wrap or stop behavior.
;   - On EndLoop/ReverseLoop finish: sets state.Flags[Param1]=true,
;     clears CYCLING bit, sets Direction=0 and CycleType=Normal.
;   - Updates VIEW_POS_CEL_NUM and calls b9SetCel.
;
; Notes:
;   - Requires constant CYCLING_INV = $FF ^ CYCLING (bitwise inverse mask).
;   - 65C02-only (uses STZ, BRA). No TRB used because TRB doesn’t support (zp),Y.

b9AdvanceCel:
.scope
    bra @start                     ; skip over jump table

; ------------------------------------------------------------------
; Jump table: CycleType * 2 → handler address
; 0 = Normal, 1 = EndLoop, 2 = ReverseLoop, 3 = Reverse
; ------------------------------------------------------------------
@b9AdvanceCelJmpTable:
    .addr @normal
    .addr @endLoop
    .addr @reverseLoop
    .addr @reverse

; ------------------------------------------------------------------
@start:
    ; Early-out if NoAdvance is set this frame; also clear it for next frame.
    ldy _offsetOfNoAdvance
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    beq @advanceCel                ; 0 = advance, non-zero = skip

@setNoAdvanceAndReturn:
    lda #$00
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    rts

; ------------------------------------------------------------------
@advanceCel:
    ; theCel ← CurrentCel (into working register)
    ldy _offsetOfCurrentCel
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta VIEW_POS_THE_CEL

    ; lastCel ← NumberOfCels - 1 (scratch)
    ldy _offsetOfNumberOfCelsVT
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    dec A
    sta VIEW_POS_LAST_CEL

    ; Dispatch to handler by CycleType
    ldy _offsetOfCycleStatus
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    asl A                          ; multiply by 2 for 16-bit table
    tay
    lda @b9AdvanceCelJmpTable,y
    sta sreg
    lda @b9AdvanceCelJmpTable+1,y
    sta sreg+1
    jmp (sreg)

; ------------------------------------------------------------------
@normal:
    ; Normal: ++cel, wrap to 0 if > lastCel
    lda VIEW_POS_THE_CEL
    inc A
    cmp VIEW_POS_LAST_CEL          ; sets carry if >= lastCel
    sta VIEW_POS_THE_CEL           ; store incremented value
    beq @setCel                    ; if == lastCel → stop increment
    bcc @setCel                    ; if < lastCel → keep value
    stz VIEW_POS_THE_CEL           ; if > lastCel → wrap to 0
    bra @setCel

; ------------------------------------------------------------------
@endLoop:
    ; EndLoop: advance until lastCel, then finish (set flag, stop cycling)
    lda VIEW_POS_THE_CEL
    cmp VIEW_POS_LAST_CEL
    bcs @endLoopAction             ; already at/past last → finish

    lda VIEW_POS_THE_CEL
    inc A
    cmp VIEW_POS_LAST_CEL
    sta VIEW_POS_THE_CEL
    bne @setCel                    ; not yet last → continue

@endLoopAction:
    ; state.Flags[Param1] = true
    ldy _offsetOfParam1
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    SET_FLAG_NON_INTERPRETER sreg

    ; Cycle=false → clear CYCLING bit in Flags
    ldy _offsetOfFlags
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    and CYCLING ^ $FF
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    ; Direction = 0
    ldy _offsetOfDirection
    lda #$00
    sta (VIEW_POS_LOCAL_VIEW_TAB),y

    ; CycleType = Normal
    ldy _offsetOfCycleStatus
    lda #MOTION_NORMAL
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    bra @setCel

; ------------------------------------------------------------------
@reverseLoop:
    ; ReverseLoop: decrement until 0, then finish (set flag, stop cycling)
    lda VIEW_POS_THE_CEL
    beq @endLoopAction             ; already 0 → finish
    dec                            ; decrement accumulator
    sta VIEW_POS_THE_CEL
    bne @setCel                    ; not yet 0 → continue
    bra @endLoopAction             ; now 0 → finish

; ------------------------------------------------------------------
@reverse:
    ; Reverse: --cel, wrap from 0 → lastCel
    lda VIEW_POS_THE_CEL
    beq @setCelToLastCel           ; if 0 → wrap
@minusCel:
    dec VIEW_POS_THE_CEL           ; else decrement
    bra @setCel
@setCelToLastCel:
    lda VIEW_POS_LAST_CEL
    sta VIEW_POS_THE_CEL           ; wrap to last

; ------------------------------------------------------------------
@setCel:
    ; Apply: write cel and call setter
    lda VIEW_POS_THE_CEL
    sta VIEW_POS_CEL_NUM
    jsr b9SetCel
@return:
    rts
.endscope

; _b9AnimateObjects
; ----------------
; Purpose:
;   Implements the two–pass object-update pipeline for all animated views.
;   Pass 1 calls b9UpdateLoopAndCel for each active object.
;   Pass 2 calls b9UpdatePositionAsm for each active object.
;
; Parity:
;   Mirrors the C# AnimateObjects() method:
;       1. For each AnimatedObject → UpdateLoopAndCel()
;       2. Reset EGOEDGE / OBJHIT / OBJEDGE
;       3. For each AnimatedObject → UpdatePosition()
;       4. Clear Ego’s “StayOnLand/StayOnWater” flags
;   The per-entry guard `(ANIMATED | UPDATE | DRAWN)` is identical to
;   the C# caller-side guard; both passes use the same condition.
;
; Calling/Assumptions:
;   - _viewtab points to the start of the 256-entry view table.
;   - VIEW_POS_LOCAL_VIEW_TAB is a zero-page pointer into that table.
;   - Each entry has fixed stride `_sizeOfViewTab`.
;   - _offsetOfFlags gives the offset of the flag byte within an entry.
;   - loopMethodToCall is patched with the target subroutine for the pass.
;   - 256 entries exactly; X rolls 0→255→0 as in AGI.
;
; Inputs:
;   - Global view table (_viewtab) containing per-object state.
;   - Each object’s flag bits determine inclusion in the update.
;
; Outputs/Side Effects:
;   - Updates loops/cels and then positions for all active objects.
;   - Clears per-frame EGOEDGE / OBJHIT / OBJEDGE variables.
;   - Clears Ego’s ONLAND/ONWATER constraint bits.
;
; Notes:
;   - Both passes share b9LoopThroughAnimatedObjects for iteration.
;   - STZ clears VIEW_POS_NEW_LOOP only in Pass 1 to match C#.
;   - Self-modifying JSR operands (loopMethodToCall) must reside in RAM.
; ---------------------------------------------------------------------

_b9AnimateObjects:
.scope
    ; Point shared loop at b9UpdateLoopAndCel
    lda #<b9UpdateLoopAndCel
    sta loopMethodToCall + 1
    lda #>b9UpdateLoopAndCel
    sta loopMethodToCall + 2
    
    jsr b9LoopThroughAnimatedObjects

@resetEgoEdgeValues:
    ; Reset per-frame variables (EGOEDGE, OBJHIT, OBJEDGE)
    ldx #$0
    lda #EGOEDGE
    SET_VAR_NON_INTERPRETER sreg
    ldx #$0
    lda #OBJHIT
    SET_VAR_NON_INTERPRETER sreg
    ldx #$0
    lda #OBJEDGE
    SET_VAR_NON_INTERPRETER sreg

    ; --- Pass 2: Update position ---
    ; lda #<b9UpdatePositionAsm
    ; sta loopMethodToCall + 1
    ; lda #>b9UpdatePositionAsm
    ; sta loopMethodToCall + 2
    jsr b9LoopThroughAnimatedObjects

    ; --- Final: clear Ego land/water bits (StayOnLand/StayOnWater) ---
    lda #<_viewtab
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda #>_viewtab
    sta VIEW_POS_LOCAL_VIEW_TAB + 1
    ; If Ego is not the first entry, advance pointer by EgoIndex * _sizeOfViewTab

    lda #(>ONLAND | >ONWATER) ^ $FF     ; Invert mask for clear
    ldy _offsetOfFlags + 1              ; High-byte flags
    and (VIEW_POS_LOCAL_VIEW_TAB),y
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    rts

; b9LoopThroughAnimatedObjects
; -----------------------------
; Purpose:
;   Iterate the 256-entry view table and invoke the method currently
;   patched into loopMethodToCall for each object whose flags include
;   ANIMATED | UPDATE | DRAWN.
;
; Parity:
;   Matches the two foreach-loops in C# AnimateObjects().
;
; Calling/Assumptions:
;   - loopMethodToCall contains address of routine to invoke.
;   - VIEW_POS_LOCAL_VIEW_TAB points into view table.
;   - _sizeOfViewTab is entry stride.
;
; Inputs:
;   - _viewtab, _offsetOfFlags, loopMethodToCall.
;
; Outputs/Side Effects:
;   - Calls the target routine for each qualifying object.
;   - Uses VIEW_POS_ENTRY_NUM as temporary to preserve X across call.
; -------------------------------------------------------------------

b9LoopThroughAnimatedObjects:
    ; Initialize table pointer
    lda #<_viewtab
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda #>_viewtab
    sta VIEW_POS_LOCAL_VIEW_TAB + 1

    ldx #$0

animatedObjectsLoop:
    ; Test for ANIMATED|UPDATE|DRAWN
    lda #ANIMATED | UPDATE | DRAWN
    ldy _offsetOfFlags
    and (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp #ANIMATED | UPDATE | DRAWN
    bne calculateNextAddress

updateLoopAndCel:
    ; Prepare entry index
    stx VIEW_POS_ENTRY_NUM

    ; Clear VIEW_POS_NEW_LOOP (harmless in Pass 2; C# parity OK)
    stz VIEW_POS_NEW_LOOP

loopMethodToCall:
    jsr b9UpdateLoopAndCel              ; Patched per pass

    ; Restore X from saved entry index
    ldx VIEW_POS_ENTRY_NUM

calculateNextAddress:
    ; Advance pointer to next entry
    clc
    lda _sizeOfViewTab
    adc VIEW_POS_LOCAL_VIEW_TAB
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda #$0
    adc VIEW_POS_LOCAL_VIEW_TAB + 1
    sta VIEW_POS_LOCAL_VIEW_TAB + 1
    inx

    cpx #VIEW_TABLE_SIZE
    bne animatedObjectsLoop

    rts
.endscope

.endif

