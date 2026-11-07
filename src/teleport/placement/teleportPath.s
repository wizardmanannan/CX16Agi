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

.segment "BANKRAM0A"
bAPathSteps: .res 1
bAPathQuietX: .res 1
bAPathQuietY: .res 1
bAPathQuiet: .res 1             ; nonzero once a quiet point was passed

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet,
; short of it), TELEPORT_UNQUIET (no quiet point on the way), or
; TELEPORT_REJECTED when the start or the first step is not walkable.
bATeleportPath:
    stz bAPathSteps
    stz bAPathQuiet
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVX
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
@probe:
    jsr bATeleportProbe
    beq @blocked
    cmp #TELEPORT_SAFE
    bne @arrived
    sta bAPathQuiet
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta bAPathQuietX
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta bAPathQuietY
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
    inc bAPathSteps
    ldy #TELEPORT_VT_XPOS
    ldx #TELEPORT_VT_PREVX
    lda _teleportX
    jsr bATeleportApproach
    ldy #TELEPORT_VT_YPOS
    ldx #TELEPORT_VT_PREVY
    lda _teleportY
    jsr bATeleportApproach
    bra @probe
@blocked:
    lda bAPathSteps
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
    lda bAPathQuiet
    beq @unquiet
    lda bAPathQuietX            ; land on the last quiet point
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_YPOS
    lda bAPathQuietY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp _teleportY
    bne @adjusted
    lda bAPathQuietX
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
bATeleportApproach:
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
