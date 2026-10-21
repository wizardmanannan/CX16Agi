.ifndef TELEPORT_LANDING_INC
TELEPORT_LANDING_INC = 1
; teleportLanding.s - Where is the nearest landing in a new room?
;
; A new room has no path from the old room's coordinates, so search outward
; from the target in rings of Manhattan distance; the first quiet point
; (teleportProbe.s) wins. Only if the room has none does a second search take
; the first walkable point. Each search probes a screen point at most once and
; needs no table. Order is fixed - radius, then x left to right, then above
; before below - so results are reproducible.

.segment "BANKRAM09"
b9LandingRadius: .res 2
b9LandingX: .res 1
b9LandingLast: .res 1
b9LandingDy: .res 1
b9LandingNeed: .res 1           ; TELEPORT_SAFE, then TELEPORT_WALKABLE

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet),
; TELEPORT_UNQUIET (walkable only), or TELEPORT_REJECTED.
b9TeleportLanding:
    lda #TELEPORT_SAFE
    sta b9LandingNeed
    jsr b9LandingSearch
    bne @done
    dec b9LandingNeed
    jsr b9LandingSearch
    beq @done
    lda #TELEPORT_UNQUIET
@done:
    rts

; Out: A = TELEPORT_PLACED, TELEPORT_ADJUSTED or TELEPORT_REJECTED; Z reflects A.
b9LandingSearch:
    stz b9LandingRadius
    stz b9LandingRadius + 1
@ring:
    lda _teleportX              ; first column: max(0, x - r)
    sec
    sbc b9LandingRadius
    tax
    lda #0
    sbc b9LandingRadius + 1
    bcs @first
    ldx #0
@first:
    stx b9LandingX
    lda _teleportX              ; last column: min(159, x + r)
    clc
    adc b9LandingRadius
    tax
    lda #0
    adc b9LandingRadius + 1
    bne @clamp
    cpx #TELEPORT_WIDTH
    bcc @last
@clamp:
    ldx #TELEPORT_WIDTH - 1
@last:
    stx b9LandingLast
@column:
    lda b9LandingX              ; dy = r - |x - target x|
    sec
    sbc _teleportX
    bcs @distance
    eor #$FF
    inc a
@distance:
    sta b9LandingDy
    lda b9LandingRadius
    sec
    sbc b9LandingDy
    tax
    lda b9LandingRadius + 1
    sbc #0
    bne @next                   ; dy >= 256: off screen both ways
    cpx #TELEPORT_HEIGHT
    bcs @next
    stx b9LandingDy
    lda _teleportY              ; above
    sec
    sbc b9LandingDy
    bcc @below
    jsr b9TeleportTryAt
    bcs @found
@below:
    lda b9LandingDy
    beq @next                   ; on the row itself: one point
    clc
    adc _teleportY
    bcs @next
    cmp #TELEPORT_HEIGHT
    bcs @next
    jsr b9TeleportTryAt
    bcs @found
@next:
    lda b9LandingX
    inc b9LandingX
    cmp b9LandingLast
    bcc @column
    inc b9LandingRadius         ; rings up to 159 + 167 cover the screen
    bne @more
    inc b9LandingRadius + 1
@more:
    lda b9LandingRadius
    cmp #<(TELEPORT_WIDTH + TELEPORT_HEIGHT - 1)
    lda b9LandingRadius + 1
    sbc #>(TELEPORT_WIDTH + TELEPORT_HEIGHT - 1)
    bcs @rejected
    jmp @ring
@rejected:
    lda #TELEPORT_REJECTED
    rts
@found:
    lda b9LandingRadius         ; radius 0 is the target itself
    ora b9LandingRadius + 1
    cmp #1
    lda #TELEPORT_PLACED
    adc #0
    rts

; Probes (b9LandingX, A). Out: carry set if good enough (b9LandingNeed).
b9TeleportTryAt:
    ldy #TELEPORT_VT_YPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    lda b9LandingX
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVX
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    jsr b9TeleportProbe
    cmp b9LandingNeed
    rts
.endif
