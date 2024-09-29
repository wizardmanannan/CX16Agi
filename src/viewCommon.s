.ifndef VIEWCOMMON_INC
; Set the value of include guard and define constants
VIEWCOMMON_INC = 1

.import _offsetOfBmp
.import _offsetOfBmpBank
.import _offsetOfCelHeight
.import _offsetOfCelTrans
.import _offsetOfSplitCelBank
.import _offsetOfCelWidth
.import _offsetOfCelHeight
.import _offsetOfSplitCelPointers
.import _offsetOfSplitSegments
.import _offsetOfXPos
.import _offsetOfYPos
.import _offsetOfPriority


;Used in both single and bulk
VERA_BYTES_PER_ROW = ZP_TMP_2
BCOL = ZP_TMP_2 + 1
VERA_ADDRESS = ZP_TMP_3
VERA_ADDRESS_HIGH = ZP_TMP_4
CEL_ADDR = ZP_TMP_5
BUFFER_STATUS = ZP_TMP_6 ;Takes Up 7 as well To be replaced
BUFFER_POINTER = ZP_TMP_8
VIEW_TAB_ADDRESS = ZP_TMP_6
CEL_HEIGHT = ZP_TMP_9
CEL_TRANS = ZP_TMP_9 + 1
OUTPUT_COLOUR = ZP_TMP_10
SPLIT_CEL_BANK = ZP_TMP_10 + 1
SPLIT_SEGMENTS = ZP_TMP_12
SPLIT_COUNTER = ZP_TMP_12 + 1
SPLIT_CEL_SEGMENTS = ZP_TMP_13
PRIORITY_VERA_ADDRESS = ZP_TMP_14
X_VAL = ZP_TMP_18 + 1 ;Ideally we would start from ZP_TMP_16 by TOTAL_ROWS cannot be moved for some reason TODO: Investigate
Y_VAL = ZP_TMP_19
P_NUM = ZP_TMP_19 + 1
X_VAL_ORIG = ZP_TMP_20 
CEL_BANK = ZP_TMP_20 + 1
NEXT_DATA_INDEX = ZP_TMP_21
BMP_BANK = ZP_TMP_21 + 1
BMP_DATA = ZP_TMP_22

;Used in bulk
NO_OF_CELS = ZP_TMP_14
BULK_ADDRESS_INDEX = ZP_TMP_14 + 1
SIZE_OF_CEL = ZP_TMP_16
CLEAR_COLOR = ZP_TMP_16 + 1
TOTAL_ROWS = ZP_TMP_17
MAX_VERA_SLOTS = ZP_TMP_17 + 1
CEL_COUNTER = ZP_TMP_18

;Color must be loaded into A
.macro SET_COLOR_LEFT TMP
asl a           ; Shift left 4 times to multiply by 16
asl a  
asl a  
asl a  
ora TMP
.endmacro

;Color must be loaded into A
.macro SET_COLOR_RIGHT TMP
lsr a           ; Shift left 4 times to multiply by 16
lsr a  
lsr a  
lsr a  
ora TMP
.endmacro

.endif