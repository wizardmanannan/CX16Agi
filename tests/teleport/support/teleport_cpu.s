; Assembles the production teleport modules and the production collision
; routines for sim65. Only VERA priority reads, bank switching and the clock
; are substituted; var[]/flag[] (and the flag stores) point at test arrays.
.setcpu "65C02"
.importzp ptr4, tmp4
.export _trampoline
.import _offsetOfFlags, _offsetOfXPos, _offsetOfYPos, _offsetOfXSize, _offsetOfYSize
.import _offsetOfPrevY, _offsetOfPriority, _sizeOfViewTab, _viewtab, _horizon
.import _b9PreComputedPriority, _testPriority
.import _testVars, _testFlags, _testJiffies
.import _testRead, _testReadByte, _testReadStatus, _testClose, _testNoMacptr, pushax
.export _teleportVtXPos: abs = TELEPORT_VT_XPOS, _teleportVtYPos: abs = TELEPORT_VT_YPOS
.export _teleportVtXSize: abs = TELEPORT_VT_XSIZE, _teleportVtYSize: abs = TELEPORT_VT_YSIZE
.export _teleportVtDirection: abs = TELEPORT_VT_DIRECTION, _teleportVtMotion: abs = TELEPORT_VT_MOTION
.export _teleportVtFlags: abs = TELEPORT_VT_FLAGS, _teleportVtStale: abs = TELEPORT_VT_STALE
.export _teleportVtPrevX: abs = TELEPORT_VT_PREVX, _teleportVtPrevY: abs = TELEPORT_VT_PREVY
.export _teleportVtSize: abs = TELEPORT_VT_SIZE
.export _b9CanBeHere

ANIMATED = $40
DRAWN = $01
IGNOREOBJECTS = $0200
FIXEDPRIORITY = $04
IGNOREBLOCKS = $02
IGNOREHORIZON = $08
ONLAND = $0800
ONWATER = $0100
HIGHEST_PRIORITY = 15
WATER = 3
VIEW_TABLE_SIZE = 20
FLAG_ON_WATER = 0
FLAG_HIT_SPECIAL = 3
PICTURE_WIDTH = 160
PICTURE_HEIGHT = 168
TELEPORT_VAR = _testVars
TELEPORT_FLAG = _testFlags
FLAG_OFFSET = _testFlags
TELEPORT_RDTIM = testJiffies
TELEPORT_CHKIN = testChkin
TELEPORT_CLRCHN = testClrchn
TELEPORT_MACPTR = testMacptr
TELEPORT_BASIN = testBasin
TELEPORT_READST = testReadst
TELEPORT_CLOSE = testClose

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

.macro GET_PRIORITY
    lda ZP_TMP_25
    ldx ZP_TMP_25 + 1
    jsr _testPriority
.endmacro
; As helpersAsm.s; sim65 memory is flat, so the bank is ignored.
.macro TRAMPOLINE BANK, JMPTO
    ldy BANK
    sty tmp4
    ldy #< JMPTO
    sty ptr4
    ldy #> JMPTO
    sty ptr4 + 1
    jsr _trampoline
.endmacro

.segment "CODE"
_trampoline:
    jmp (ptr4)
testJiffies:
    lda _testJiffies
    ldx _testJiffies + 1
    rts
; KERNAL file input over the C file services (teleport_services.c). The
; real KERNAL leaves cc65's zero page alone; C does not, so save what the
; teleport read loop keeps there.
.importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3
; Saves cc65's scratch zero page around a C call, as the KERNAL never uses it.
.macro KERNAL_STUB target
    .local @save, @restore
    sta testResult              ; the argument, if any
    ldx #0
@save:
    lda ptr1,x
    pha
    inx
    cpx #6                      ; ptr1, ptr2, ptr3
    bne @save
    lda tmp1
    pha
    lda tmp2
    pha
    lda tmp3
    pha
    lda testResult
    jsr target
    sta testResult
    pla
    sta tmp3
    pla
    sta tmp2
    pla
    sta tmp1
    ldx #5
@restore:
    pla
    sta ptr1,x
    dex
    bpl @restore
    lda testResult
.endmacro
testChkin:
testClrchn:
    rts
testMacptr:                     ; A bytes to X/Y; X = bytes read, C set if unsupported
    sta testCount
    lda _testNoMacptr
    beq @supported
    sec
    rts
@supported:
    stx testDestination
    sty testDestination + 1
    KERNAL_STUB testReadBlock
    tax
    ldy #0
    clc
    rts
testReadBlock:
    lda testDestination
    ldx testDestination + 1
    jsr pushax
    lda testCount
    jmp _testRead
testBasin:
    KERNAL_STUB _testReadByte
    rts
testReadst:
    KERNAL_STUB _testReadStatus
    cmp #0
    rts
testClose:
    KERNAL_STUB _testClose
    rts
testDestination: .res 2
testResult: .res 1
testCount: .res 1
.include "collision.inc"
.include "teleport/core/teleportState.s"
.include "teleport/command/teleportKeyword.s"
.include "teleport/command/teleportEntry.s"
.include "teleport/command/teleportNotice.s"
.include "teleport/placement/teleportView.s"
.include "teleport/placement/teleportProbe.s"
.include "teleport/placement/teleportPath.s"
.include "teleport/placement/teleportLanding.s"
.include "teleport/placement/teleportPublish.s"
.include "teleport/placement/teleportPlace.s"
.include "teleport/placement/teleportPark.s"
.include "teleport/placement/teleportArrival.s"
.include "teleport/command/teleportGrammar.s"
.include "teleport/search/teleportDirectory.s"
.include "teleport/command/teleportText.s"
.include "teleport/search/teleportScan.s"
.include "teleport/search/teleportRoomSearch.s"
.include "teleport/search/teleportTriggers.s"
.include "teleport/tour/teleportTour.s"
.include "teleport/command/teleportCommand.s"

; Hooks for the scanner differential test (test_teleport_rooms.c).
.import popax
.importzp ptr1
.export _testSetLine, _testScanReset, _testScan, _testScanEnd
.segment "CODE"
_testSetLine:                   ; void testSetLine(const char* line)
    sta bFTeleportLine
    stx bFTeleportLine + 1
    rts
_testScanReset:                 ; void testScanReset(void)
    jmp bFTeleportScanReset
_testScan:                      ; void __fastcall__ testScan(const byte* data, byte length)
    pha
    jsr popax
    sta ptr1
    stx ptr1 + 1
    pla
    jmp bFTeleportScan
_testScanEnd:                   ; void testScanEnd(void)
    jmp bFTeleportScanEnd
