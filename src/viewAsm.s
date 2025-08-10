.ifndef  VIEW_INC
VIEW_INC = 1

.include "viewFlags.s"

.import _offsetOfFlags
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfPrevY
.import _viewtab
.import _sizeOfViewTab
.import _viewTabFastLookup

.segment "BANKRAM09"
;boolean b9Collide(ViewTable* localViewtab, byte entryNum, byte localCelWidth) 
_b9Collide:
.scope
LOCAL_VIEW_TAB = ZP_TMP_2
LOCAL_CEL_WIDTH = ZP_TMP_3
ENTRY_NUM = ZP_TMP_3 + 1
LOCAL_VIEW_FLAGS = ZP_TMP_4
OTHER_VIEW_FLAGS = ZP_TMP_5
OTHER_VIEW_TAB = ZP_TMP_6

sta LOCAL_CEL_WIDTH
jsr popa 
sta ENTRY_NUM
jsr popax
sta LOCAL_VIEW_TAB
stx LOCAL_VIEW_TAB

ldy _offsetOfFlags
lda (LOCAL_VIEW_TAB),y
sta LOCAL_VIEW_FLAGS
iny
sta (LOCAL_VIEW_TAB),y

and #>IGNORE_OBJECTS
bne @returnFalse

ldy #$0

@viewTabLoop:
tya
asl

stp
tax
lda _viewTabFastLookup,x
sta OTHER_VIEW_TAB
inx
lda _viewTabFastLookup,x
sta OTHER_VIEW_TAB + 1



iny

@viewTabCheck:
cpy #VIEW_TABLE_SIZE
bcc @viewTabLoop

@return:

@returnFalse:
; lda #$0
rts

.endscope

.endif