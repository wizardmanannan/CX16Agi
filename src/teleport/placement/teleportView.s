.ifndef TELEPORT_VIEW_INC
TELEPORT_VIEW_INC = 1
; teleportView.s - What do ego's view entry and the collision rules say?
;
; Everything teleport does with the view table or the interpreter's collision
; routines, which live in bank 9 with it. Placement itself works on a copy of
; ego at VIEW_POS_LOCAL_VIEW_TAB.
.import _controlMode
.export _b9TeleportUnpark

TELEPORT_OP_BNE = $D0
TELEPORT_OP_BRA = $80

.segment "BANKRAM09"
; Out: carry clear if ego can be placed: player control, moving normally,
; drawn and animated, with a sane cel size.
b9TeleportCanPlace:
    lda _controlMode
    ora _controlMode + 1
    ora _viewtab + TELEPORT_VT_MOTION
    ora _viewtab + TELEPORT_VT_XPOS + 1
    ora _viewtab + TELEPORT_VT_YPOS + 1
    ora _viewtab + TELEPORT_VT_XSIZE + 1
    ora _viewtab + TELEPORT_VT_YSIZE + 1
    cmp #1                      ; any of them nonzero: carry set
    bcs @done
    lda _viewtab + TELEPORT_VT_FLAGS
    and #TELEPORT_EGO_ACTIVE
    cmp #TELEPORT_EGO_ACTIVE
    sec
    bne @done
    lda _viewtab + TELEPORT_VT_XSIZE
    dec a                       ; 1..160 -> 0..159
    cmp #TELEPORT_WIDTH
    bcs @done
    lda _viewtab + TELEPORT_VT_YSIZE
    dec a
    cmp #TELEPORT_HEIGHT
@done:
    rts

; Copies ego's view entry to the probe at VIEW_POS_LOCAL_VIEW_TAB.
b9TeleportCopyEgo:
    ldy #TELEPORT_VT_SIZE - 1
@copy:
    lda _viewtab,y
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    dey
    bpl @copy
    rts

; Out: A = 0 if the probe breaks the interpreter's movement rules - screen
; bounds and horizon, other objects, control lines, water; Z reflects A.
b9TeleportWalkable:
    jsr b9GoodPositionAsm
    beq @done
    stz VIEW_POS_ENTRY_NUM      ; objects, skipping ego's own entry
    jsr b9CollideAsm
    eor #1
    beq @done
    sta VIEW_POS_ENTRY_NUM      ; 1: control lines without ego's flag writes
    jmp b9CanBeHereAsm
@done:
    rts

; Out: A = ego's cel width.
b9TeleportEgoWidth:
    lda _viewtab + TELEPORT_VT_XSIZE
    rts

; Puts ego at (A, X) as if it had stopped there.
b9TeleportMoveEgo:
    sta _viewtab + TELEPORT_VT_XPOS
    sta _viewtab + TELEPORT_VT_PREVX
    stz _viewtab + TELEPORT_VT_XPOS + 1
    stx _viewtab + TELEPORT_VT_YPOS
    stx _viewtab + TELEPORT_VT_PREVY
    stz _viewtab + TELEPORT_VT_YPOS + 1
    rts

; CanBeHere stops writing ego's water and trigger flags: its "is this ego?"
; branch becomes BRA. Costs nothing when no teleport is in progress.
b9TeleportHoldEgoFlags:
    lda #TELEPORT_OP_BRA
    .byte $2C                   ; bit abs: skip the lda
; void b9TeleportUnpark(void): CanBeHere sets ego's flags again.
_b9TeleportUnpark:
    lda #TELEPORT_OP_BNE
    sta b9CanBeHereEgoFlags
    rts
.endif
