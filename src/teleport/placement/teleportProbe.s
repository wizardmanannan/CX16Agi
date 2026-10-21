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

.segment "BANKRAM09"
b9ProbeGround: .res 1
b9ProbeLeft: .res 1
b9ProbeRight: .res 1
b9ProbeY: .res 1
b9TeleportZones: .res 2         ; count, then x1 y1 x2 y2 per box

.scope
X_VAL = ZP_TMP_25               ; GET_PRIORITY operands
Y_VAL = ZP_TMP_25 + 1
TELEPORT_BACKGROUND = 4

quit:                           ; A = 0, in branch range of the tests
    rts

; Out: A = 0 (not walkable), TELEPORT_WALKABLE or TELEPORT_SAFE. Z reflects A.
::b9TeleportProbe:
    jsr b9TeleportWalkable
    beq quit
    lda #TELEPORT_SAFE
    sta b9ProbeGround
    ldy #TELEPORT_VT_YPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta Y_VAL
    sta b9ProbeY
    ldy #TELEPORT_VT_XPOS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta X_VAL
    sta b9ProbeLeft
    ldy #TELEPORT_VT_XSIZE
    clc
    adc (VIEW_POS_LOCAL_VIEW_TAB),y
    sta VIEW_POS_WIDTH
    dec a
    sta b9ProbeRight
@pixel:
    GET_PRIORITY
    cmp #TELEPORT_BACKGROUND
    beq @next
    bcs @scenery
    lda #TELEPORT_WALKABLE      ; trigger line or water underfoot
    sta b9ProbeGround
@next:
    inc X_VAL
    lda X_VAL
    cmp VIEW_POS_WIDTH
    bne @pixel
    lda b9ProbeGround
    cmp #TELEPORT_SAFE
    bne @done
    jsr b9TeleportZoneHit
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

; Carry set if ego's baseline (b9ProbeLeft..b9ProbeRight at b9ProbeY) is in
; any box at b9TeleportZones.
b9TeleportZoneHit:
    lda b9TeleportZones
    sta ptr1
    lda b9TeleportZones + 1
    sta ptr1 + 1
    lda (ptr1)
    beq @clear
    tax
    ldy #1
@zone:
    lda b9ProbeRight            ; x1 <= right
    cmp (ptr1),y
    bcc @miss
    iny
    lda b9ProbeY                ; y1 <= y
    cmp (ptr1),y
    bcc @miss
    iny
    lda (ptr1),y                ; left <= x2
    cmp b9ProbeLeft
    bcc @miss
    iny
    lda (ptr1),y                ; y <= y2
    cmp b9ProbeY
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
