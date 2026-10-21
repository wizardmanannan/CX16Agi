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

.segment "BANKRAM0A"
bALandingRadius: .res 2
bALandingX: .res 1
bALandingLast: .res 1
bALandingDy: .res 1
bALandingNeed: .res 1           ; TELEPORT_SAFE, then TELEPORT_WALKABLE

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet),
; TELEPORT_UNQUIET (walkable only), or TELEPORT_REJECTED.
bATeleportLanding:
    lda #TELEPORT_SAFE
    sta bALandingNeed
    jsr bALandingSearch
    bne @done
    dec bALandingNeed
    jsr bALandingSearch
    beq @done
    lda #TELEPORT_UNQUIET
@done:
    rts

; Out: A = TELEPORT_PLACED, TELEPORT_ADJUSTED or TELEPORT_REJECTED; Z reflects A.
bALandingSearch:
    stz bALandingRadius
    stz bALandingRadius + 1
@ring:
    lda _teleportX              ; first column: max(0, x - r)
    sec
    sbc bALandingRadius
    tax
    lda #0
    sbc bALandingRadius + 1
    bcs @first
    ldx #0
@first:
    stx bALandingX
    lda _teleportX              ; last column: min(159, x + r)
    clc
    adc bALandingRadius
    tax
    lda #0
    adc bALandingRadius + 1
    bne @clamp
    cpx #TELEPORT_WIDTH
    bcc @last
@clamp:
    ldx #TELEPORT_WIDTH - 1
@last:
    stx bALandingLast
@column:
    lda bALandingX              ; dy = r - |x - target x|
    sec
    sbc _teleportX
    bcs @distance
    eor #$FF
    inc a
@distance:
    sta bALandingDy
    lda bALandingRadius
    sec
    sbc bALandingDy
    tax
    lda bALandingRadius + 1
    sbc #0
    bne @next                   ; dy >= 256: off screen both ways
    cpx #TELEPORT_HEIGHT
    bcs @next
    stx bALandingDy
    lda _teleportY              ; above
    sec
    sbc bALandingDy
    bcc @below
    jsr bATeleportTryAt
    bcs @found
@below:
    lda bALandingDy
    beq @next                   ; on the row itself: one point
    clc
    adc _teleportY
    bcs @next
    cmp #TELEPORT_HEIGHT
    bcs @next
    jsr bATeleportTryAt
    bcs @found
@next:
    lda bALandingX
    inc bALandingX
    cmp bALandingLast
    bcc @column
    inc bALandingRadius         ; rings up to 159 + 167 cover the screen
    bne @more
    inc bALandingRadius + 1
@more:
    lda bALandingRadius
    cmp #<(TELEPORT_WIDTH + TELEPORT_HEIGHT - 1)
    lda bALandingRadius + 1
    sbc #>(TELEPORT_WIDTH + TELEPORT_HEIGHT - 1)
    bcs @rejected
    jmp @ring
@rejected:
    lda #TELEPORT_REJECTED
    rts
@found:
    lda bALandingRadius         ; radius 0 is the target itself
    ora bALandingRadius + 1
    cmp #1
    lda #TELEPORT_PLACED
    adc #0
    rts

; Probes (bALandingX, A). Out: carry set if good enough (bALandingNeed).
bATeleportTryAt:
    ldy #TELEPORT_VT_YPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVY
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    lda bALandingX
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    ldy #TELEPORT_VT_PREVX
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    jsr bATeleportProbe
    cmp bALandingNeed
    rts
.endif
