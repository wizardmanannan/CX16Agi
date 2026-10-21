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
; While ego is parked, CanBeHere leaves ego's water and trigger flags alone
; (teleportView.s) until b9TeleportUnpark.
.importzp c_sp
.import subysp, addysp
.export _bATeleportPark

TELEPORT_PARK_STEP = 12         ; grid tried after the target

.segment "BANKRAM0A"
bAParkX: .res 1
bAParkY: .res 1
bAParkWidth: .res 1

; void b9TeleportPark(void): before entering room teleportPending.
_bATeleportPark:
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportHoldEgoFlags
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportEgoWidth
    sta bAParkWidth
    stz TELEPORT_FLAG + 0       ; ego on water: the old room's
    stz TELEPORT_FLAG + 3       ; ego touched a trigger line
    ldy #TELEPORT_ZONE_BYTES
    jsr subysp
    lda c_sp
    sta bATeleportZones
    ldx c_sp + 1
    stx bATeleportZones + 1
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
    jsr bATeleportParkAt
    bcc @free
    lda #TELEPORT_HEIGHT - 8    ; then a grid, bottom up
    sta bAParkY
@row:
    lda #4
    sta bAParkX
@column:
    lda bAParkX
    ldx bAParkY
    jsr bATeleportParkAt
    bcc @free
    lda bAParkX
    clc
    adc #TELEPORT_PARK_STEP
    sta bAParkX
    cmp #TELEPORT_WIDTH - 8
    bcc @column
    lda bAParkY
    sec
    sbc #TELEPORT_PARK_STEP
    sta bAParkY
    cmp #40
    bcs @row                    ; nowhere free: ego stays where it was
@free:
    ldy #TELEPORT_ZONE_BYTES
    jmp addysp

; Parks ego at (A, X) unless that is in a box. Carry set if it is.
bATeleportParkAt:
    sta bAProbeLeft
    stx bAProbeY
    clc
    adc bAParkWidth
    dec a
    sta bAProbeRight
    jsr bATeleportZoneHit
    bcs @done
    lda bAProbeLeft
    ldx bAProbeY
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportMoveEgo
    rts
@done:
    rts
.endif
