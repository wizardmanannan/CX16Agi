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


;boolean b9CanBeHere(ViewTable* localViewtab, byte entryNum)
_b9CanBeHere:
.scope
X_VAL = ZP_TMP_25
Y_VAL = ZP_TMP_25 + 1
sta VIEW_POS_ENTRY_NUM

jsr popax
sta VIEW_POS_LOCAL_VIEW_TAB
stx VIEW_POS_LOCAL_VIEW_TAB + 1

lda #$1
sta VIEW_POS_CAN_BE_HERE
stz VIEW_POS_ENTIRELY_ON_WATER
stz VIEW_POS_HIT_SPECIAL

ldy _offsetOfFlags
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta VIEW_POS_FLAGS_LOW
and #FIXEDPRIORITY
bne @checkIfHighestPriority

ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
tay

lda _b9PreComputedPriority,y

ldy _offsetOfPriority
sta (VIEW_POS_LOCAL_VIEW_TAB),y

@checkIfHighestPriority:
ldy _offsetOfPriority
lda (VIEW_POS_LOCAL_VIEW_TAB),y
cmp #HIGHEST_PRIORITY
bne @calculateIfCanBeHere
jmp @setEgoWaterSpecialFlags

@calculateIfCanBeHere:
inc VIEW_POS_ENTIRELY_ON_WATER

ldy _offsetOfXPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y

sta X_VAL

ldy _offsetOfYPos
lda (VIEW_POS_LOCAL_VIEW_TAB),y
sta Y_VAL

ldy _offsetOfXSize
lda (VIEW_POS_LOCAL_VIEW_TAB),y

clc
adc X_VAL
sta VIEW_POS_WIDTH
lda X_VAL
bra @postIncrement

@checkAgainstControlLoopCheck:
lda X_VAL
inc

@postIncrement:
cmp VIEW_POS_WIDTH
beq @endCheckAgainstControlLoop
; cmp #PICTURE_WIDTH
; bcs @endCheckAgainstControlLoop

sta X_VAL

GET_PRIORITY

cmp #WATER
beq @checkAgainstControlLoopCheck
stz VIEW_POS_ENTIRELY_ON_WATER

@checkPriority0:
cmp #$0
bne @checkPriority1
stz VIEW_POS_CAN_BE_HERE
bra @checkAgainstControlLoopCheck

@checkPriority1:
cmp #$1
bne @checkPriority2
tax
lda VIEW_POS_FLAGS_LOW

and #IGNOREBLOCKS
bne @checkAgainstControlLoopCheck
stz VIEW_POS_CAN_BE_HERE
bra @endCheckAgainstControlLoop

@checkPriority2:
cmp #2
bne @checkAgainstControlLoopCheck
lda #$1
sta VIEW_POS_HIT_SPECIAL
bra @checkAgainstControlLoopCheck

@endCheckAgainstControlLoop:

@checkEntirelyOnWater:
lda VIEW_POS_ENTIRELY_ON_WATER
beq @checkHitSpecial
ldy _offsetOfFlags
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #>ONLAND
beq @setEgoWaterSpecialFlags
stz VIEW_POS_CAN_BE_HERE
bra @setEgoWaterSpecialFlags

@checkHitSpecial:
ldy _offsetOfFlags
iny
lda (VIEW_POS_LOCAL_VIEW_TAB),y
and #>ONWATER
beq @setEgoWaterSpecialFlags
stz VIEW_POS_CAN_BE_HERE

@setEgoWaterSpecialFlags:
lda VIEW_POS_ENTRY_NUM
bne @return

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

@return:
lda VIEW_POS_CAN_BE_HERE
rts
.endscope

.endif