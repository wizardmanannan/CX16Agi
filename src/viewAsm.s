.ifndef  VIEW_INC
VIEW_INC = 1

.import _offsetOfFlags
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfXSize
.import _offsetOfYSize
.import _offsetOfPrevY
.import _viewtab
.import _sizeOfViewTab
.import _viewTabFastLookup

.segment "BANKRAM09"
;boolean b9Collide(ViewTable* localViewtab, byte entryNum) 
_b9Collide:
.scope
LOCAL_VIEW_TAB = ZP_TMP_2
ENTRY_NUM = ZP_TMP_3 + 1
LOCAL_VIEW_FLAGS = ZP_TMP_4
OTHER_VIEW_FLAGS = ZP_TMP_5
OTHER_VIEW_TAB = ZP_TMP_6
LOOP_COUNTER = ZP_TMP_7
LOCAL_VIEW_TAB_Y = ZP_TMP_7 + 1
OTHER_VIEW_TAB_Y = ZP_TMP_8
LOCAL_VIEW_TAB_PREV_Y = ZP_TMP_8 + 1
OTHER_VIEW_TAB_PREV_Y = ZP_TMP_9

sta ENTRY_NUM
jsr popax
sta LOCAL_VIEW_TAB
stx LOCAL_VIEW_TAB + 1

ldy _offsetOfFlags
lda (LOCAL_VIEW_TAB),y
sta LOCAL_VIEW_FLAGS
iny
lda (LOCAL_VIEW_TAB),y
sta LOCAL_VIEW_FLAGS
and #>IGNOREOBJECTS
beq @loadInitialOtherViewtab
jmp @returnFalse

@loadInitialOtherViewtab:
lda #<_viewtab
sta OTHER_VIEW_TAB
lda #>_viewtab
sta OTHER_VIEW_TAB + 1

stz LOOP_COUNTER

@viewTabLoop:
ldy _offsetOfFlags
lda (OTHER_VIEW_TAB),y
tax

and #ANIMATED
beq @viewTabCheck

txa
and #DRAWN
beq @viewTabCheck

iny
lda (OTHER_VIEW_TAB),y
and #>IGNOREOBJECTS
bne @viewTabCheck

lda ENTRY_NUM
cmp LOOP_COUNTER
beq @viewTabCheck

ldy _offsetOfXPos ;We are going with the assumption that not colliding is less common than colliding, therefore it makes more sense to not take extra cycles by store the results for the next calculation
lda (LOCAL_VIEW_TAB),y
clc
ldy _offsetOfXSize
adc (LOCAL_VIEW_TAB),y
ldy _offsetOfXPos
cmp (OTHER_VIEW_TAB),y
bcc @returnFalse

clc
lda (OTHER_VIEW_TAB),y
ldy _offsetOfXSize
adc (OTHER_VIEW_TAB),y
ldy _offsetOfXPos
cmp (LOCAL_VIEW_TAB),y
beq @viewTabCheck
bcc @viewTabCheck

ldy _offsetOfYPos
lda (OTHER_VIEW_TAB),y
cmp (LOCAL_VIEW_TAB),y
beq @returnTrue
bcs @localYLess

;For these two we have already checked this: localViewtab->yPos > otherViewTab.yPos and localViewtab->yPos < otherViewTab.yPos 
@localYGreater:
ldy _offsetOfPrevY
lda (LOCAL_VIEW_TAB),y
cmp (OTHER_VIEW_TAB),y
bcs @returnFalse
lda #$1
rts

@localYLess:
ldy _offsetOfPrevY
lda (LOCAL_VIEW_TAB),y
cmp (OTHER_VIEW_TAB),y
beq @returnFalse
bcc @returnFalse

@returnTrue:
lda #$1
rts

@checkY:


@viewTabCheck:
clc
lda _sizeOfViewTab
adc OTHER_VIEW_TAB
sta OTHER_VIEW_TAB
lda #$0
adc OTHER_VIEW_TAB + 1
sta OTHER_VIEW_TAB + 1

inc LOOP_COUNTER
lda LOOP_COUNTER
cmp #VIEW_TABLE_SIZE
bcs @return
jmp @viewTabLoop

@return:

@returnFalse:
lda #$0
rts

.endscope

.endif