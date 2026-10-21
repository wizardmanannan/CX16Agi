.ifndef TELEPORT_PLACE_INC
TELEPORT_PLACE_INC = 1
; teleportPlace.s - Can ego be placed now, and where?
;
; Ego must be under player control, moving normally, drawn and animated, with
; a sane cel size. The room script's armed position triggers are read first
; (teleportTriggers.s) so the landing avoids them. A same-room request walks
; the path to the target; a new room (flag 5) searches for a landing. The work
; is done on copies on the C stack - ego, then the trigger boxes - so a
; rejection leaves ego and the world flags untouched.
.importzp c_sp
.import subysp, addysp
.export _b9TryTeleport

TELEPORT_PLACE_SCRATCH = TELEPORT_VT_SIZE + TELEPORT_ZONE_BYTES

.segment "BANKRAM09"
; byte b9TryTeleport(void): TELEPORT_REJECTED, _PLACED, _ADJUSTED or _UNQUIET.
_b9TryTeleport:
    jsr b9TeleportCanPlace
    bcs @reject
    ldy #TELEPORT_PLACE_SCRATCH
    jsr subysp
    lda c_sp
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda c_sp + 1
    sta VIEW_POS_LOCAL_VIEW_TAB + 1
    jsr b9TeleportCopyEgo
    lda c_sp                    ; trigger boxes follow the copy
    clc
    adc #TELEPORT_VT_SIZE
    sta b9TeleportZones
    lda c_sp + 1
    adc #0
    sta b9TeleportZones + 1
    tax
    lda b9TeleportZones
    TRAMPOLINE #TELEPORT_BANK, _bFTeleportTriggers
    lda c_sp
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda c_sp + 1
    sta VIEW_POS_LOCAL_VIEW_TAB + 1

    lda TELEPORT_FLAG + 5       ; new room
    beq @path
    jsr b9TeleportLanding
    bra @placed
@path:
    jsr b9TeleportPath
@placed:
    cmp #TELEPORT_REJECTED
    beq @free
    jsr b9TeleportPublish
@free:
    ldx #0
    ldy #TELEPORT_PLACE_SCRATCH
    jmp addysp                  ; preserves A
@reject:
    lda #TELEPORT_REJECTED
    tax
    rts
.endif
