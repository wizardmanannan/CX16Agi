.ifndef TELEPORT_PATH_INC
TELEPORT_PATH_INC = 1
; teleportPath.s - How far can ego go along a straight line in this room?
;
; Steps the probe one pixel per probe - diagonally until one axis matches -
; from where ego stands toward the target, stopping at the target or before
; the first point that is not walkable; no wall is ever crossed and there are
; at most 168 probes. Ego lands on the last quiet point passed (often the
; target itself), so the jump starts nothing. Previous X/Y track the last
; walkable point, as ordinary movement leaves them for the collision test.
.importzp tmp1

.segment "BANKRAM09"
b9PathSteps: .res 1
b9PathQuietX: .res 1
b9PathQuietY: .res 1
b9PathQuiet: .res 1             ; nonzero once a quiet point was passed

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet,
; short of it), TELEPORT_UNQUIET (no quiet point on the way), or
; TELEPORT_REJECTED when the start or the first step is not walkable.
b9TeleportPath:
    stz b9PathSteps
    stz b9PathQuiet
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVX
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
@probe:
    jsr b9TeleportProbe
    beq @blocked
    cmp #TELEPORT_SAFE
    bne @arrived
    sta b9PathQuiet
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta b9PathQuietX
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta b9PathQuietY
@arrived:
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp _teleportX
    bne @step
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp _teleportY
    beq @end
@step:
    inc b9PathSteps
    ldy #TELEPORT_VT_XPOS
    ldx #TELEPORT_VT_PREVX
    lda _teleportX
    jsr b9TeleportApproach
    ldy #TELEPORT_VT_YPOS
    ldx #TELEPORT_VT_PREVY
    lda _teleportY
    jsr b9TeleportApproach
    bra @probe
@blocked:
    lda b9PathSteps
    cmp #2
    bcc @rejected               ; 0: start invalid; 1: first step invalid
    ldy #TELEPORT_VT_PREVX      ; back to the last walkable point
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
@end:
    lda b9PathQuiet
    beq @unquiet
    lda b9PathQuietX            ; land on the last quiet point
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    lda b9PathQuietY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp _teleportY
    bne @adjusted
    lda b9PathQuietX
    cmp _teleportX
    bne @adjusted
    lda #TELEPORT_PLACED
    rts
@adjusted:
    lda #TELEPORT_ADJUSTED
    rts
@unquiet:
    lda #TELEPORT_UNQUIET
    rts
@rejected:
    lda #TELEPORT_REJECTED
    rts

; Copies the coordinate at offset Y to offset X (previous), then moves it one
; pixel toward A.
b9TeleportApproach:
    sta tmp1
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    phy
    phx
    ply
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ply
    cmp tmp1
    beq @done
    bcc @up
    dec a
    bra @store
@up:
    inc a
@store:
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
@done:
    rts
.endif
