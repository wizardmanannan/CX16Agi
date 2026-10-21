.ifndef TELEPORT_PARK_INC
TELEPORT_PARK_INC = 1
; teleportPark.s - Where can ego wait while the new room sets itself up?
;
; new.room keeps ego's old-room coordinates, and the new room's script runs a
; whole cycle - initialization and its event checks - before teleport places
; ego. Old coordinates can fall in the new room's river or trigger boxes and
; start a scene or kill ego before placement. So, before entering, ego is
; parked outside every box of the destination script that could fire (read
; as if already there), and the stale water/trigger flags are cleared.
; The new picture is not drawn yet, so the parked spot may be water: the
; room's own draw would then set the water flag and its script start a swim.
; While ego is parked, CanBeHere therefore leaves ego's water and trigger
; flags alone: its "is this ego?" branch becomes BRA until b9TeleportUnpark.
; Patching the branch costs nothing when no teleport is in progress.
.importzp c_sp
.import subysp, addysp
.export _b9TeleportPark, _b9TeleportUnpark

TELEPORT_PARK_STEP = 12         ; grid tried after the target
TELEPORT_OP_BNE = $D0
TELEPORT_OP_BRA = $80

.segment "BANKRAM09"
b9ParkX: .res 1
b9ParkY: .res 1

; void b9TeleportPark(void): before entering room teleportPending.
_b9TeleportPark:
    lda #TELEPORT_OP_BRA
    sta b9CanBeHereEgoFlags
    stz TELEPORT_FLAG + 0       ; ego on water: the old room's
    stz TELEPORT_FLAG + 3       ; ego touched a trigger line
    ldy #TELEPORT_ZONE_BYTES
    jsr subysp
    lda c_sp
    sta b9TeleportZones
    ldx c_sp + 1
    stx b9TeleportZones + 1
    ldy TELEPORT_VAR + 0        ; read the destination as if there
    phy
    ldy _teleportPending
    sty TELEPORT_VAR + 0
    TRAMPOLINE #TELEPORT_BANK, _bFTeleportTriggers
    pla
    sta TELEPORT_VAR + 0
    lda _teleportX              ; first choice: the target
    ldx _teleportY
    cmp #TELEPORT_AUTO
    bne @target
    lda #TELEPORT_AUTO_X
    ldx #120
@target:
    jsr b9TeleportParkAt
    bcc @free
    lda #TELEPORT_HEIGHT - 8    ; then a grid, bottom up
    sta b9ParkY
@row:
    lda #4
    sta b9ParkX
@column:
    lda b9ParkX
    ldx b9ParkY
    jsr b9TeleportParkAt
    bcc @free
    lda b9ParkX
    clc
    adc #TELEPORT_PARK_STEP
    sta b9ParkX
    cmp #TELEPORT_WIDTH - 8
    bcc @column
    lda b9ParkY
    sec
    sbc #TELEPORT_PARK_STEP
    sta b9ParkY
    cmp #40
    bcs @row                    ; nowhere free: ego stays where it was
@free:
    ldy #TELEPORT_ZONE_BYTES
    jmp addysp

; void b9TeleportUnpark(void): CanBeHere sets ego's flags again.
_b9TeleportUnpark:
    lda #TELEPORT_OP_BNE
    sta b9CanBeHereEgoFlags
    rts

; Parks ego at (A, X) unless that is in a box. Carry set if it is.
b9TeleportParkAt:
    sta b9ProbeLeft
    stx b9ProbeY
    clc
    adc _viewtab + TELEPORT_VT_XSIZE
    dec a
    sta b9ProbeRight
    jsr b9TeleportZoneHit
    bcs @done
    lda b9ProbeLeft
    sta _viewtab + TELEPORT_VT_XPOS
    sta _viewtab + TELEPORT_VT_PREVX
    stz _viewtab + TELEPORT_VT_XPOS + 1
    lda b9ProbeY
    sta _viewtab + TELEPORT_VT_YPOS
    sta _viewtab + TELEPORT_VT_PREVY
    stz _viewtab + TELEPORT_VT_YPOS + 1
@done:
    rts
.endif
