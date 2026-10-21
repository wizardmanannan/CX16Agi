.ifndef TELEPORT_ARRIVAL_INC
TELEPORT_ARRIVAL_INC = 1
; teleportArrival.s - What happens when the requested room is reached?
;
; Called by the interpreter cycle (bank 6). A request for another room parks
; ego where the new room's script cannot react (teleportPark.s), then enters it
; through the engine's own new.room, so the room script draws its picture and
; initializes; its message boxes are skipped while teleportPending is set
; (commands.c). Placement follows once that has run (flag 5). A request for the
; current room is placed at once.
.import _b6NewRoom, _newRoomNum, _horizon, _dirnOfEgo
.export _b6BeginTeleport, _b6FinishTeleport

.segment "BANKRAM06"
; void b6BeginTeleport(void): teleportPending is nonzero.
_b6BeginTeleport:
    lda _teleportPending
    cmp TELEPORT_VAR + 0
    beq _b6FinishTeleport
    TRAMPOLINE #TELEPORT_PLACE_BANK, _b9TeleportPark
    lda _teleportPending
    stz TELEPORT_VAR + 2        ; no entry edge: ego stays parked
    stz TELEPORT_VAR + 6
    stz _dirnOfEgo
    stz _dirnOfEgo + 1
    sta _newRoomNum
    stz _newRoomNum + 1
    jmp _b6NewRoom

; void b6FinishTeleport(void): the requested room has initialized (or is current).
; A redirect by the room script consumes the request without placement.
_b6FinishTeleport:
    TRAMPOLINE #TELEPORT_PLACE_BANK, _b9TeleportUnpark
    ldx #TELEPORT_WRONG_ROOM
    lda _teleportPending
    stz _teleportPending
    cmp TELEPORT_VAR + 0
    bne @report
    ldx #0
    lda _teleportX
    inc a
    bne @explicit               ; X Y were given
    lda TELEPORT_FLAG + 5
    beq @report                 ; automatic, already here: leave ego alone
    lda #TELEPORT_AUTO_X        ; automatic: search out from the middle of
    sta _teleportX              ; the walkable area, without a notice
    lda _horizon
    clc
    adc #TELEPORT_HEIGHT
    ror a
    sta _teleportY
    lda #0
    .byte $2C                   ; bit abs: skip the lda
@explicit:
    lda #TELEPORT_NEAREST
    pha                         ; notice for an adjusted landing
    TRAMPOLINE #TELEPORT_PLACE_BANK, _b9TryTeleport
    plx
    cmp #TELEPORT_ADJUSTED
    beq @report
    ldx #0
    cmp #TELEPORT_PLACED
    beq @report
    ldx #TELEPORT_EXPOSED
    cmp #TELEPORT_UNQUIET
    beq @report
    ldx #TELEPORT_BLOCKED
@report:
    lda _teleportTour           ; a tour moves on, without notices
    beq @notice
    TRAMPOLINE #TELEPORT_BANK, _bFTeleportArrived
    rts
@notice:
    txa
    beq @done
    TRAMPOLINE #TELEPORT_NOTICE_BANK, _b7TeleportNotice
@done:
    rts
.endif
