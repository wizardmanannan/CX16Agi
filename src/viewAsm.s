.ifndef  VIEW_INC
VIEW_INC = 1

.import _offsetOfFlags
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfXSize
.import _offsetOfYSize
.import _offsetOfPrevY
.import _offsetOfPriority
.import _viewtab
.import _sizeOfViewTab
.import _viewTabFastLookup
.import _b9PreComputedPriority
.import _horizon

VIEW_POS_LOCAL_VIEW_TAB = ZP_TMP_2
VIEW_POS_ENTRY_NUM = ZP_TMP_3 + 1
VIEW_POS_LOCAL_VIEW_FLAGS = ZP_TMP_4
VIEW_POS_OTHER_VIEW_FLAGS = ZP_TMP_5
VIEW_POS_OTHER_VIEW_TAB = ZP_TMP_6
VIEW_POS_VIEW_POS_OTHER_VIEW_TAB = ZP_TMP_7
VIEW_POS_LOCAL_VIEW_TAB_Y = ZP_TMP_7 + 1
VIEW_POS_OTHER_VIEW_TAB_Y = ZP_TMP_8
VIEW_POS_LOCAL_VIEW_TAB_PREV_Y = ZP_TMP_8 + 1
VIEW_POS_OTHER_VIEW_TAB_PREV_Y = ZP_TMP_9
VIEW_POS_CAN_BE_HERE = ZP_TMP_9 + 1
VIEW_POS_ENTIRELY_ON_WATER = ZP_TMP_10
VIEW_POS_HIT_SPECIAL = ZP_TMP_10 + 1
VIEW_POS_WIDTH = ZP_TMP_12
VIEW_POS_FLAGS_LOW = ZP_TMP_12 + 1
VIEW_POS_FLAGS_HIGH = ZP_TMP_13 
;Don't put anything in 25 used for x and y of canBeHere

.segment "BANKRAM09"
;boolean b9Collide(ViewTable* localViewtab, byte entryNum) 
_b9Collide:
.scope

sta VIEW_POS_ENTRY_NUM
jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1

ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_LOCAL_VIEW_FLAGS
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_LOCAL_VIEW_FLAGS
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

.endscope


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

_b9CanBeHere:
.scope
X_VAL = ZP_TMP_25                 ; scratch: current X being scanned along baseline
Y_VAL = ZP_TMP_25 + 1             ; scratch: Y of the baseline
sta VIEW_POS_ENTRY_NUM            ; A = object/entry index (ego == 0)

jsr popax                         ; pop pointer to view table into A/X
sta VIEW_POS_LOCAL_VIEW_TAB       ;   low byte of pointer
stx VIEW_POS_LOCAL_VIEW_TAB + 1   ;   high byte

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


;boolean b6GoodPosition(Viewtab* localViewTab)
_b6GoodPosition:
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1

ldy _offsetOfXPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
ldy _offsetOfXSize
adc (VIEW_POS_LOCAL_VIEW_TAB),y

cmp #PICTURE_WIDTH
beq @checkY
bcs @returnFalse

@checkY:
ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
tax

sec
ldy _offsetOfYSize
sbc (VIEW_POS_LOCAL_VIEW_TAB),y
bvc @checkLessThanMaxY
cmp #$FF
bne @returnFalse

@checkLessThanMaxY:
txa
cmp #PICTURE_HEIGHT
bcs @returnFalse

ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #IGNOREHORIZON
bne @returnTrue

@returnTrue:
lda #$1
rts

@returnFalse:
lda #$0
rts
.endif
