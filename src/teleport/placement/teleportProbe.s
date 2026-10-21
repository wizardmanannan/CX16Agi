.ifndef TELEPORT_PROBE_INC
TELEPORT_PROBE_INC = 1
; teleportProbe.s - Can ego stand here, and would standing here start anything?
;
; Judges the probe copy of ego at VIEW_POS_LOCAL_VIEW_TAB. Walkable means the
; interpreter's own movement rules - screen bounds and horizon, other objects,
; control lines and water - plus teleport's landing rule that no baseline
; pixel is raised scenery, so ego never lands in front of a wall. Safe means
; walkable and also quiet: every baseline pixel is plain background (no
; trigger line, no water) and ego is outside every room-script box that could
; fire now (teleportTriggers.s). Nothing is published while probing.
.importzp ptr1

.segment "BANKRAM0A"
bAProbeGround: .res 1
bAProbeLeft: .res 1
bAProbeRight: .res 1
bAProbeY: .res 1
bATeleportZones: .res 2         ; count, then x1 y1 x2 y2 per box

.scope
X_VAL = ZP_TMP_25               ; GET_PRIORITY operands
Y_VAL = ZP_TMP_25 + 1
TELEPORT_BACKGROUND = 4

quit:                           ; A = 0, in branch range of the tests
    rts

; Out: A = 0 (not walkable), TELEPORT_WALKABLE or TELEPORT_SAFE. Z reflects A.
::bATeleportProbe:
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportWalkable
    beq quit
    lda #TELEPORT_SAFE
    sta bAProbeGround
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta Y_VAL
    sta bAProbeY
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta X_VAL
    sta bAProbeLeft
    ldy #TELEPORT_VT_XSIZE
    clc
    adc (VIEW_POS_LOCAL_VIEW_TAB),y
    sta VIEW_POS_WIDTH
    dec a
    sta bAProbeRight
@pixel:
    GET_PRIORITY
    cmp #TELEPORT_BACKGROUND
    beq @next
    bcs @scenery
    lda #TELEPORT_WALKABLE      ; trigger line or water underfoot
    sta bAProbeGround
@next:
    inc X_VAL
    lda X_VAL
    cmp VIEW_POS_WIDTH
    bne @pixel
    lda bAProbeGround
    cmp #TELEPORT_SAFE
    bne @done
    jsr bATeleportZoneHit
    lda #TELEPORT_WALKABLE      ; standing here could start a scene
    bcs @done
@safe:
    lda #TELEPORT_SAFE
@done:
    rts
@scenery:
    lda #0
    rts
.endscope

; Carry set if ego's baseline (bAProbeLeft..bAProbeRight at bAProbeY) is in
; any box at bATeleportZones.
bATeleportZoneHit:
    lda bATeleportZones
    sta ptr1
    lda bATeleportZones + 1
    sta ptr1 + 1
    lda (ptr1)
    beq @clear
    tax
    ldy #1
@zone:
    lda bAProbeRight            ; x1 <= right
    cmp (ptr1),y
    bcc @miss
    iny
    lda bAProbeY                ; y1 <= y
    cmp (ptr1),y
    bcc @miss
    iny
    lda (ptr1),y                ; left <= x2
    cmp bAProbeLeft
    bcc @miss
    iny
    lda (ptr1),y                ; y <= y2
    cmp bAProbeY
    bcs @hit
@miss:
    tya                         ; next box: 4k+1 .. 4k+4 -> 4k+5
    dec a
    ora #3
    inc a
    inc a
    tay
    dex
    bne @zone
@clear:
    clc
@hit:
    rts
.endif
