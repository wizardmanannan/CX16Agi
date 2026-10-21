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
.importzp c_sp, ptr1, ptr2, tmp1, tmp2
.import subysp, addysp

TELEPORT_ROW_BYTES = TELEPORT_WIDTH / 2
TELEPORT_BACKGROUND = 4
TELEPORT_NO_BEST = $FF          ; best high byte: nothing found yet
TELEPORT_QUIET = 0              ; levels: index into the best tables
TELEPORT_ANY = 1                ; ... walkable is enough

.segment "BANKRAM0A"
bALandingLevel: .res 1          ; TELEPORT_QUIET or TELEPORT_ANY
bALandingNeed: .res 1           ; probe result it needs
bALandingMask: .res 1           ; its window flag
bALandingWalkLow: .res 1        ; lowest priority a walkable pixel may have
bALandingWidth: .res 1
bALandingDy: .res 1
bALandingY: .res 1
bALandingX: .res 1
bALandingDx: .res 1
bALandingLimit: .res 1          ; largest |dx| still worth probing on the row
bALandingLast: .res 1           ; borrowed while clearing a box
bALandingAny: .res 1            ; window flags seen on the row
bALandingDistance: .res 2
bALandingBestLow: .res 2        ; per level: distance of the best point,
bALandingBestHigh: .res 2       ; ... TELEPORT_NO_BEST if none
bALandingBestX: .res 2
bALandingBestY: .res 2

; Out: A = TELEPORT_PLACED (quiet, at the target), TELEPORT_ADJUSTED (quiet),
; TELEPORT_UNQUIET (walkable only), or TELEPORT_REJECTED.
; One sweep finds both: the nearest quiet point and, while no quiet point
; is known, the nearest walkable one - the rows a quiet search must visit
; anyway.
bATeleportLanding:
    lda _teleportX              ; the target itself, before any setup
    sta bALandingX
    lda _teleportY
    sta bALandingY
    jsr bATeleportTryAt
    cmp #TELEPORT_SAFE
    bne @search
    lda #TELEPORT_PLACED
    rts
@search:
    pha                         ; the target's result
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
    pla
    jsr bALandingSearch
    ldy #TELEPORT_WIDTH
    jmp addysp                  ; preserves A

; In: A = the target's probe result, not quiet; bALandingX/Y the target.
bALandingSearch:
    ldx #TELEPORT_NO_BEST
    stx bALandingBestHigh + TELEPORT_QUIET
    stx bALandingBestHigh + TELEPORT_ANY
    cmp #TELEPORT_WALKABLE
    bne @rows
    stz bALandingDy             ; walkable here: the nearest possible
    stz bALandingDx
    ldx #TELEPORT_ANY
    jsr bALandingKeep
@rows:
    stz bALandingDy
@row:
    lda bALandingBestHigh + TELEPORT_QUIET
    bne @visit                  ; no quiet point yet (or 256 away): every row
    lda bALandingBestLow + TELEPORT_QUIET ; rows past it cannot win
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
    ldx #TELEPORT_QUIET
    lda bALandingBestHigh,x
    cmp #TELEPORT_NO_BEST
    bne @best
    inx
    lda bALandingBestHigh,x
    cmp #TELEPORT_NO_BEST
    beq @rejected
@best:
    lda bALandingBestX,x        ; the probe ends on the winner
    sta bALandingX
    lda bALandingBestY,x
    phx
    jsr bATeleportTryAt
    plx
    bne @walkable
    lda #TELEPORT_ADJUSTED      ; quiet, and not the target (it was probed)
    rts
@walkable:
    lda #TELEPORT_UNQUIET
    rts
@rejected:
    lda #TELEPORT_REJECTED
    rts

; Row A: for each level still open, its nearest passing point, kept if it
; beats that level's best.
bALandingRow:
    sta bALandingY
    ldy #TELEPORT_VT_YPOS       ; a row ego cannot stand on at all
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    lda #0
    ldy #TELEPORT_VT_XPOS
    sta (VIEW_POS_LOCAL_VIEW_TAB),y
    TRAMPOLINE #TELEPORT_VIEW_BANK, b9TeleportInBounds
    beq @done
    jsr bALandingWindows
    lda bALandingAny
    beq @done
    ldx #TELEPORT_QUIET
    jsr bALandingLevelRow
    lda bALandingBestHigh + TELEPORT_QUIET
    cmp #TELEPORT_NO_BEST
    bne @done                   ; quiet found: walkable no longer matters
    ldx #TELEPORT_ANY
    jmp bALandingLevelRow
@done:
    rts

; Level X on row bALandingY: its nearest passing point within the level's
; best distance.
bALandingLevelRow:
    stx bALandingLevel
    txa                         ; mask: 1 quiet, 2 walkable
    inc a
    and bALandingAny
    beq @done                   ; no such window on the row
    sta bALandingMask
    lda #TELEPORT_SAFE          ; need: quiet, then walkable
    sec
    sbc bALandingLevel
    sta bALandingNeed
    lda #TELEPORT_WIDTH - 1     ; |dx| <= best - |dy|
    ldy bALandingBestHigh,x
    bne @limit                  ; nothing yet, or 256 and more
    lda bALandingBestLow,x
    sec
    sbc bALandingDy
    bcc @done
    cmp #TELEPORT_WIDTH
    bcc @limit
    lda #TELEPORT_WIDTH - 1
@limit:
    sta bALandingLimit
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
@found:
    ldx bALandingLevel
    ; Fall through.

; Keeps (bALandingX, bALandingY), dy + dx away, as level X's best if it is
; nearer, or as near and further left.
bALandingKeep:
    lda bALandingDy             ; at most 326
    clc
    adc bALandingDx
    sta bALandingDistance
    lda #0
    rol a
    sta bALandingDistance + 1
    cmp bALandingBestHigh,x     ; nothing yet: $FF, always more
    bcc @better
    bne @done
    lda bALandingDistance
    cmp bALandingBestLow,x
    bcc @better
    bne @done
    lda bALandingX
    cmp bALandingBestX,x
    bcs @done
@better:
    lda bALandingDistance
    sta bALandingBestLow,x
    lda bALandingDistance + 1
    sta bALandingBestHigh,x
    lda bALandingX
    sta bALandingBestX,x
    lda bALandingY
    sta bALandingBestY,x
@done:
    rts

; Row bALandingY at (ptr2), one byte per X, becomes window flags for the
; window of pixels X - width + 1 .. X: bit 1 if all are walkable (walk low
; .. background), bit 0 if all are plain background and ego there is outside
; every trigger box. A window is found by its last pixel, from running
; counts of passing pixels.
bALandingWindows:
    ldy bALandingY
    jsr TELEPORT_ROW_READER
    stz bALandingAny
    stz tmp1                    ; plain background pixels in a row
    lda bALandingWidth
    sta tmp2
    ldx #0                      ; walkable pixels in a row
    ldy #0
    lda bALandingBestHigh + TELEPORT_QUIET
    cmp #TELEPORT_NO_BEST
    bne @quiet                  ; quiet found: walkable no longer matters
    lda bALandingBestHigh + TELEPORT_ANY
    bne @pixel                  ; nothing walkable yet (or 256 away)
    lda bALandingBestLow + TELEPORT_ANY
    cmp bALandingDy
    bcs @pixel                  ; a nearer walkable point may be here
@quiet:                         ; quiet windows only: X counts background
    lda (ptr2),y
    cmp #TELEPORT_BACKGROUND
    bne @busy
    inx
    .byte $2C                   ; bit abs: skip the ldx
@busy:
    ldx #0
    cpx tmp2
    lda #0
    rol a
    beq @empty
    tsb bALandingAny
@empty:
    sta (ptr2),y
    iny
    cpy #TELEPORT_WIDTH
    bne @quiet
    bra @boxes
@pixel:
    lda (ptr2),y
    cmp #TELEPORT_BACKGROUND
    beq @plain                  ; the common case: both runs grow
    bcs @fail                   ; raised scenery
    cmp bALandingWalkLow
    bcc @fail
    inx
    stz tmp1
    bra @flag
@plain:
    inx
    inc tmp1
    bra @flag
@fail:
    ldx #0
    stz tmp1
@flag:
    lda #0
    cpx tmp2
    bcc @store                  ; not walkable, so not quiet either
    lda tmp1
    cmp tmp2
    lda #1
    rol a                       ; bit 1 walkable, bit 0 quiet
    tsb bALandingAny
@store:
    sta (ptr2),y
    iny
    cpy #TELEPORT_WIDTH
    bne @pixel
@boxes:
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
@clear:
    lda (ptr2),y                ; not quiet
    and #$FE
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
    and bALandingMask
    beq @no
    lda bALandingY
    jsr bATeleportTryAt
    cmp bALandingNeed
    rts
@no:
    clc
    rts

; Probes (bALandingX, A). Out: A = the probe's result.
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
    jmp bATeleportProbe

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
