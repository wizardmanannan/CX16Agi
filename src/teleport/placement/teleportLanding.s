.ifndef TELEPORT_LANDING_INC
TELEPORT_LANDING_INC = 1
; teleportLanding.s - Where is the nearest landing in a new room?
;
; A new room has no path from the old room's coordinates, so ego lands on the
; point nearest the target by Manhattan distance - ties to the smaller X,
; then above before below - that is quiet (teleportProbe.s), or, if the room
; has none, merely walkable.
;
; Rows are visited outward from the target row and the search stops at the
; first row farther away than the best point found. Each row's priority line
; is read from VERA once and reduced in place, with a running count, to the
; X positions whose whole baseline could pass; for a quiet landing the room's
; trigger boxes on that row are removed too. Only those positions get a full
; probe, nearest first, and the first that passes is the row's best. The full
; probe decides, so the result is the one probing every point in distance
; order would give, at a fraction of the probes.
.importzp c_sp, ptr1, ptr2
.import subysp, addysp

TELEPORT_ROW_BYTES = TELEPORT_WIDTH / 2
TELEPORT_BACKGROUND = 4
TELEPORT_NO_BEST = $FF          ; bALandingBest + 1: nothing found yet

.segment "BANKRAM0A"
bALandingNeed: .res 1           ; TELEPORT_SAFE, then TELEPORT_WALKABLE
bALandingWalkLow: .res 1        ; lowest priority a walkable pixel may have
bALandingLow: .res 1            ; ... for this search
bALandingWidth: .res 1
bALandingDy: .res 1
bALandingY: .res 1
bALandingX: .res 1
bALandingDx: .res 1
bALandingLimit: .res 1          ; largest |dx| still worth probing on the row
bALandingLast: .res 1           ; borrowed while clearing a box
bALandingAny: .res 1            ; nonzero if any window on the row passed
bALandingBest: .res 2           ; distance of the best point
bALandingBestX: .res 1
bALandingBestY: .res 1

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet),
; TELEPORT_UNQUIET (walkable only), or TELEPORT_REJECTED.
bATeleportLanding:
    ldy #TELEPORT_WIDTH         ; the row: one byte per X
    jsr subysp
    lda c_sp
    sta ptr2
    lda c_sp + 1
    sta ptr2 + 1
    ldy #TELEPORT_VT_XSIZE
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    sta bALandingWidth
    ldx #2                      ; CanBeHere refuses 0 and 1 underfoot,
    ldy #TELEPORT_VT_FLAGS
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    pha
    and #IGNOREBLOCKS           ; 1 only without ignore.blocks,
    beq @blocks
    dex
@blocks:
    pla
    and #FIXEDPRIORITY          ; and nothing at a fixed priority of 15
    beq @low
    ldy #TELEPORT_VT_PRIORITY
    lda (VIEW_POS_LOCAL_VIEW_TAB),y
    cmp #HIGHEST_PRIORITY
    bne @low
    ldx #0
@low:
    stx bALandingWalkLow
    lda #TELEPORT_SAFE
    sta bALandingNeed
    jsr bALandingSearch
    bne @done
    dec bALandingNeed
    jsr bALandingSearch
    beq @done
    lda #TELEPORT_UNQUIET
@done:
    ldy #TELEPORT_WIDTH
    jmp addysp                  ; preserves A

; Out: A = TELEPORT_PLACED, TELEPORT_ADJUSTED or TELEPORT_REJECTED; Z reflects A.
bALandingSearch:
    lda #TELEPORT_NO_BEST
    sta bALandingBest + 1
    lda bALandingWalkLow        ; quiet: plain background only
    ldx bALandingNeed
    cpx #TELEPORT_SAFE
    bne @low
    lda #TELEPORT_BACKGROUND
@low:
    sta bALandingLow
    lda _teleportX              ; the target itself, before any row work
    sta bALandingX
    lda _teleportY
    jsr bATeleportTryAt
    lda #TELEPORT_PLACED
    bcs @placed
    stz bALandingDy
@row:
    lda bALandingBest + 1       ; rows past the best distance cannot win
    bne @visit
    lda bALandingBest
    cmp bALandingDy
    bcc @end
@visit:
    lda _teleportY              ; above
    sec
    sbc bALandingDy
    bcc @below
    jsr bALandingRow
@below:
    lda bALandingDy
    beq @next                   ; the target row once
    clc
    adc _teleportY
    bcs @next
    cmp #TELEPORT_HEIGHT
    bcs @next
    jsr bALandingRow
@next:
    inc bALandingDy
    lda bALandingDy
    cmp #TELEPORT_HEIGHT
    bcc @row
@end:
    lda bALandingBest + 1
    cmp #TELEPORT_NO_BEST
    beq @rejected
    lda bALandingBestX          ; the probe ends on the winner
    sta bALandingX
    lda bALandingBestY
    jsr bATeleportTryAt
    lda bALandingBest           ; distance 0 is the target itself
    ora bALandingBest + 1
    cmp #1
    lda #TELEPORT_PLACED
    adc #0
@placed:
    rts
@rejected:
    lda #TELEPORT_REJECTED
    rts

; Row A: its nearest passing point, kept if it beats the best so far.
bALandingRow:
    sta bALandingY
    ldy #TELEPORT_VT_YPOS       ; a row ego cannot stand on at all
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    lda #0
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportInBounds
    beq @done
    lda #TELEPORT_WIDTH - 1     ; |dx| <= best - |dy|
    ldx bALandingBest + 1
    bne @limit                  ; nothing yet, or 256 and more
    lda bALandingBest
    sec
    sbc bALandingDy
    cmp #TELEPORT_WIDTH
    bcc @limit
    lda #TELEPORT_WIDTH - 1
@limit:
    sta bALandingLimit
    jsr bALandingWindows
    lda bALandingAny
    beq @done
    stz bALandingDx
@dx:
    lda _teleportX              ; left first: ties go to the smaller X
    sec
    sbc bALandingDx
    bcc @right
    jsr bALandingCandidate
    bcs @found
@right:
    lda bALandingDx
    beq @wider
    clc
    adc _teleportX
    bcs @wider
    jsr bALandingCandidate
    bcs @found
@wider:
    lda bALandingDx
    cmp bALandingLimit
    inc bALandingDx
    bcc @dx
@done:
    rts
@found:                         ; distance dy + dx, at most 326
    lda bALandingDy
    clc
    adc bALandingDx
    tax
    lda #0
    adc #0
    cmp bALandingBest + 1       ; nothing yet: high byte $FF, always more
    bcc @better
    bne @done
    cpx bALandingBest
    bcc @better
    bne @done
    lda bALandingX              ; equally near: the smaller X
    cmp bALandingBestX
    bcs @done
    lda bALandingBest + 1
@better:
    stx bALandingBest
    sta bALandingBest + 1
    lda bALandingX
    sta bALandingBestX
    lda bALandingY
    sta bALandingBestY
    rts

; Row bALandingY at (ptr2), one byte per X, becomes window flags: X is
; nonzero if pixels X - width + 1 .. X all lie in bALandingLow ..
; background and, for a quiet landing, ego ending there is outside every
; trigger box. A window is found by its last pixel, so each pixel costs one
; compare against the running count of passing pixels in X.
bALandingWindows:
    ldy bALandingY
    jsr TELEPORT_ROW_READER
    stz bALandingAny
    ldx #0
    ldy #0
@pixel:
    lda (ptr2),y
    cmp #TELEPORT_BACKGROUND + 1
    bcs @fail
    cmp bALandingLow
    bcc @fail
    inx
    bra @flag
@fail:
    ldx #0
@flag:
    cpx bALandingWidth
    lda #0
    rol a
    sta (ptr2),y
    tsb bALandingAny
    iny
    cpy #TELEPORT_WIDTH
    bne @pixel
    lda bALandingNeed
    cmp #TELEPORT_SAFE
    bne @done
    lda bATeleportZones         ; count, then x1 y1 x2 y2
    sta ptr1
    lda bATeleportZones + 1
    sta ptr1 + 1
    lda (ptr1)
    beq @done
    tax
    ldy #1
@zone:
    iny                         ; y1 <= row
    lda bALandingY
    cmp (ptr1),y
    bcc @skip
    iny
    iny                         ; row <= y2
    lda (ptr1),y
    cmp bALandingY
    bcc @skip
    dey                         ; windows ending at x1 .. x2 + width - 1
    lda (ptr1),y
    clc
    adc bALandingWidth
    bcs @edge
    dec a
    cmp #TELEPORT_WIDTH
    bcc @last
@edge:
    lda #TELEPORT_WIDTH - 1
@last:
    sta bALandingLast
    dey
    dey
    phy
    lda (ptr1),y
    tay
    lda #0
@clear:
    sta (ptr2),y
    cpy bALandingLast
    iny
    bcc @clear
    ply
@skip:
    tya                         ; next box: 4k+1 .. 4k+4 -> 4k+5
    dec a
    ora #3
    inc a
    inc a
    tay
    dex
    bne @zone
@done:
    rts

; Is X = A a landing that passes bALandingNeed on row bALandingY? Carry set
; if so; the probe is left there.
bALandingCandidate:
    sta bALandingX
    clc                         ; the window ends at X + width - 1, on screen
    adc bALandingWidth
    bcs @no
    dec a
    cmp #TELEPORT_WIDTH
    bcs @no
    tay
    lda (ptr2),y
    beq @no
    lda bALandingY
    jmp bATeleportTryAt
@no:
    clc
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

.ifndef TELEPORT_ROW_READER
TELEPORT_ROW_READER = bATeleportReadRow
; Reads priority row Y to (ptr2), one byte per X. Interrupts are off only
; while VERA's port is borrowed.
bATeleportReadRow:
    sei
    VERA_CTRL_SET #0
    lda lineTablePriorityLow,y
    sta VERA_addr_low
    lda lineTablePriorityHigh,y
    sta VERA_addr_high
    lda #$10                    ; increment 1
    sta VERA_addr_bank
    ldy #0
@byte:
    lda VERA_data0              ; even X in the high nibble
    tax
    lsr a
    lsr a
    lsr a
    lsr a
    sta (ptr2),y
    iny
    txa
    and #$0F
    sta (ptr2),y
    iny
    cpy #TELEPORT_WIDTH
    bne @byte
    stz VERA_addr_bank          ; as GET_PRIORITY leaves it
    REENABLE_INTERRUPTS
    rts
.endif
.endif
