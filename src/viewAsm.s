
.ifndef VIEW_INC
; Set the value of include guard and define constants
VIEW_INC = 1

.include "globalViews.s"
.include "global.s"

.import _b5RefreshBuffer
.import _getLoadedView
.import popax
.import popa
.import pushax

.segment "BANKRAM09"
_viewHeaderBuffer: .res VIEW_HEADER_BUFFER_SIZE
_loopHeaderBuffer: .res LOOP_HEADER_BUFFER_SIZE

;View* localView , byte* localLoop, byte* localCel, byte x, byte y, byte pNum, byte bCol
CNUM = ZP_TMP
XCO = ZP_TMP_2
YCO = ZP_TMP_3
PNUM = ZP_TMP_4
BCOL = ZP_TMP_5
LOCAL_CEL = ZP_TMP_8
BUFFER_STATUS = ZP_TMP_9 ;Takes Up 10 as well
BUFFER_POINTER = ZP_TMP_12
CEL_BMP_OFFSET = 3
CEL_BANK_OFFSET = 5


_b9ViewToVera:
sta BCOL
jsr popax
sta PNUM
stx YCO
jsr popax
sta XCO
stx LOCAL_CEL
jsr popa
sta LOCAL_CEL + 1

GET_STRUCT_16 CEL_BMP_OFFSET, LOCAL_CEL, BUFFER_STATUS
GET_STRUCT_8 CEL_BANK_OFFSET, LOCAL_CEL, BUFFER_STATUS + 2

stz BUFFER_STATUS + 3

REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS

;GET_NEXT BUFFER_POINTER, BUFFER_STATUS

stp

rts
.endif


