.setcpu "65C02"
.import popax
.importzp ptr4
.export _trampoline, _b9Collide, _b9CanBeHere
.import _offsetOfFlags, _offsetOfXPos, _offsetOfYPos, _offsetOfXSize
.import _offsetOfPrevY, _offsetOfPriority, _sizeOfViewTab, _viewtab
.import _b9PreComputedPriority, _flag, _testPriority

ANIMATED = $40
DRAWN = $01
IGNOREOBJECTS = $0200
FIXEDPRIORITY = $04
IGNOREBLOCKS = $02
ONLAND = $0800
ONWATER = $0100
HIGHEST_PRIORITY = 15
WATER = 3
VIEW_TABLE_SIZE = 20
FLAG_ON_WATER = 0
FLAG_HIT_SPECIAL = 3

.segment "ZEROPAGE"
VIEW_POS_LOCAL_VIEW_TAB: .res 2
VIEW_POS_ENTRY_NUM: .res 1
VIEW_POS_LOCAL_VIEW_FLAGS: .res 2
VIEW_POS_OTHER_VIEW_TAB: .res 2
VIEW_POS_VIEW_POS_OTHER_VIEW_TAB: .res 1
VIEW_POS_CAN_BE_HERE: .res 1
VIEW_POS_ENTIRELY_ON_WATER: .res 1
VIEW_POS_HIT_SPECIAL: .res 1
VIEW_POS_WIDTH: .res 1
VIEW_POS_FLAGS_LOW: .res 1
ZP_TMP_25: .res 2
TEST_FLAG_PTR: .res 2

; Substitute only VERA reads and the global flag store macro.
.macro GET_PRIORITY
    lda ZP_TMP_25
    ldx ZP_TMP_25 + 1
    jsr _testPriority
.endmacro
.macro SET_FLAG_NON_INTERPRETER ignored
    tay
    lda _flag
    sta TEST_FLAG_PTR
    lda _flag + 1
    sta TEST_FLAG_PTR + 1
    lda #1
    sta (TEST_FLAG_PTR),y
.endmacro
.segment "CODE"
; sim65 has flat memory; preserve the cc65 wrapped-call ABI without banking.
_trampoline:
    jmp (ptr4)
.include "collision.inc"
