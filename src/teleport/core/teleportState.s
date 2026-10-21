.ifndef TELEPORT_STATE_INC
TELEPORT_STATE_INC = 1
; teleportState.s - What teleport request is active?
;
; The only teleport state outside bank 0xF: four low-RAM bytes read by the
; interpreter cycle (bank 6), placement (bank 9) and the command bank (0xF).
; Also the constants every teleport module shares.
.setcpu "65C02"
.export _teleportPending, _teleportX, _teleportY, _teleportTour

; Banks
TELEPORT_BANK = $0F             ; command, room search, tour
TELEPORT_NOTICE_BANK = $07      ; parser input and message texts
TELEPORT_PLACE_BANK = $0A       ; placement
TELEPORT_VIEW_BANK = $09        ; view table and collision
TELEPORT_ROOM_BANK = $06        ; interpreter cycle, room entry, file seek
TELEPORT_TEXT_BANK = $03        ; message boxes
TELEPORT_KEYS_BANK = $05        ; key waits
TELEPORT_DIRECTORY_BANK = $10   ; logdir, picdir
TELEPORT_TRIGGER_BANK = $01     ; the trigger reader

TELEPORT_AUTO = 255             ; teleportX: choose the landing automatically
TELEPORT_AUTO_X = 76            ; automatic search origin, X
TELEPORT_WIDTH = 160
TELEPORT_HEIGHT = 168
TELEPORT_ZONE_BYTES = 1 + 4 * 63 ; trigger boxes: count and up to 63 (Y-indexed)
TELEPORT_CODE_BUFFER = 64       ; logic code read at a time

; Notices (b7TeleportNotice)
TELEPORT_BLOCKED = 1
TELEPORT_WRONG_ROOM = 2
TELEPORT_CANCELED = 3
TELEPORT_NEAREST = 4
TELEPORT_EXPOSED = 5            ; no quiet spot: placed where the room may react
TELEPORT_UNCHECKED = 6          ; placed, but the room's triggers were not all seen

; b9TryTeleport results
TELEPORT_REJECTED = 0
TELEPORT_PLACED = 1
TELEPORT_ADJUSTED = 2
TELEPORT_UNQUIET = 3            ; walkable, but on a trigger, water or script box
TELEPORT_PARTIAL = 4            ; placed, but the room's triggers were not all seen

; b9TeleportProbe results
TELEPORT_WALKABLE = 1
TELEPORT_SAFE = 2

; ViewTable layout (graphics.h). The CPU tests check these against offsetof.
TELEPORT_VT_XPOS = 2
TELEPORT_VT_YPOS = 4
TELEPORT_VT_XSIZE = 16
TELEPORT_VT_YSIZE = 18
TELEPORT_VT_DIRECTION = 23
TELEPORT_VT_MOTION = 24
TELEPORT_VT_PRIORITY = 26
TELEPORT_VT_FLAGS = 27
TELEPORT_VT_STALE = 34
TELEPORT_VT_PREVX = 36
TELEPORT_VT_PREVY = 37
TELEPORT_VT_SIZE = 39
; ANIMATED | DRAWN | UPDATE | MOTION: ego must be on screen and moving normally.
TELEPORT_EGO_ACTIVE = $D1

; AGI file LFN and channel (general.h)
TELEPORT_LFN = 2

; var[] and flag[] live at fixed GOLDEN_RAM addresses; the CPU tests point
; these at their own arrays.
.ifndef TELEPORT_VAR
TELEPORT_VAR = SET_VAR_OFFSET
TELEPORT_FLAG = FLAG_OFFSET
.endif
.ifndef TELEPORT_RDTIM          ; KERNAL entries; the CPU tests replace them
TELEPORT_RDTIM = $FFDE          ; A/X/Y = 60 Hz jiffy clock
TELEPORT_CHKIN = $FFC6          ; X = input channel
TELEPORT_CLRCHN = $FFCC
TELEPORT_MACPTR = $FF44         ; block read: A bytes to X/Y; C set if unsupported
TELEPORT_BASIN = $FFCF
TELEPORT_READST = $FFB7
TELEPORT_CLOSE = $FFC3          ; A = logical file
.endif

; TRAMPOLINE as a tail call: JMPTO in BANK returns to our caller.
.macro TELEPORT_JUMP BANK, JMPTO
    ldy BANK
    sty tmp4
    ldy #<JMPTO
    sty ptr4
    ldy #>JMPTO
    sty ptr4 + 1
    jmp _trampoline
.endmacro

; Teleport text is ASCII (message boxes); ca65 would translate to PETSCII.
.macro TELEPORT_ASCII
    .pushcharmap
    .repeat 95, i
        .charmap i + 32, i + 32
    .endrepeat
.endmacro

.segment "BSS"
_teleportPending: .res 1        ; destination room; 0 = no request
_teleportX: .res 1              ; target X, or TELEPORT_AUTO
_teleportY: .res 1              ; target Y (baseline)
_teleportTour: .res 1           ; nonzero while a tour runs
.endif
