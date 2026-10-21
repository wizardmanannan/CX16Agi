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
.export _bATryTeleport

.segment "BANKRAM0A"
bAPlacePartial: .res 1

; Room var[0]'s trigger boxes, in a list put on the C stack (the caller
; frees TELEPORT_ZONE_BYTES). Out: A nonzero if the walk was partial.
bAReadTriggers:
    ldy #TELEPORT_ZONE_BYTES
    jsr subysp
    lda c_sp
    sta bATeleportZones
    ldx c_sp + 1
    stx bATeleportZones + 1
    TELEPORT_JUMP #TELEPORT_TRIGGER_BANK, _b1TeleportTriggers

; byte bATryTeleport(void): TELEPORT_REJECTED, _PLACED, _ADJUSTED, _UNQUIET
; or _PARTIAL.
_bATryTeleport:
    ldy #TELEPORT_VT_SIZE       ; ego's copy
    jsr subysp
    lda c_sp
    sta VIEW_POS_LOCAL_VIEW_TAB
    lda c_sp + 1
    sta VIEW_POS_LOCAL_VIEW_TAB + 1
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportTakeEgo
    bcc @take
    lda #TELEPORT_REJECTED
    bra @copy
@take:
    jsr bAReadTriggers          ; then the trigger boxes
    sta bAPlacePartial

    lda TELEPORT_FLAG + 5       ; new room
    beq @path
    jsr bATeleportLanding
    bra @placed
@path:
    jsr bATeleportPath
@placed:
    cmp #TELEPORT_REJECTED
    beq @free
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportPublish
    cmp #TELEPORT_UNQUIET
    beq @free
    ldx bAPlacePartial          ; quiet only as far as the walk could see
    beq @free
    lda #TELEPORT_PARTIAL
@free:
    ldy #TELEPORT_ZONE_BYTES
    jsr addysp                  ; preserves A
@copy:
    ldx #0
    ldy #TELEPORT_VT_SIZE
    jmp addysp
.endif
