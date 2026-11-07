.ifndef TELEPORT_PUBLISH_INC
TELEPORT_PUBLISH_INC = 1
; teleportPublish.s - How does ego take the accepted position?
;
; The probe copy becomes ego. Only the final spot's environment counts: the
; water and trigger flags are cleared, then set by one CanBeHere at the
; destination. Movement state is reset as if ego had just stopped there, and
; direction keys held before the jump are forgotten.
.import _dirnOfEgo, _b7Directions

.segment "BANKRAM09"
; In: VIEW_POS_LOCAL_VIEW_TAB = accepted probe. Preserves A.
b9TeleportPublish:
    pha
    stz TELEPORT_FLAG + 0       ; ego on water
    stz TELEPORT_FLAG + 3       ; ego touched a trigger line
    stz VIEW_POS_ENTRY_NUM
    jsr b9CanBeHereAsm          ; priority and flags at the destination
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVX
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    lda #0
    ldy #TELEPORT_VT_DIRECTION
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    inc a
    ldy #TELEPORT_VT_STALE      ; redraw once more, as after any stop
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_SIZE - 1
@copy:
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta _viewtab,y
    dey
    bpl @copy
    stz TELEPORT_VAR + 2        ; ego's entry edge
    stz TELEPORT_VAR + 6        ; ego's direction
    stz _dirnOfEgo
    stz _dirnOfEgo + 1
    ldx #8
@keys:
    stz _b7Directions,x         ; low-RAM data despite the name
    dex
    bpl @keys
    pla
    rts
.endif
