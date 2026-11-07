.ifndef TELEPORT_TOUR_INC
TELEPORT_TOUR_INC = 1
; teleportTour.s - When does the next stop start?
;
; A tour visits the parsed stops in order, waiting the requested seconds after
; each arrival - placed, rejected or redirected - so the room's picture, sound
; and opening script can be checked. The 16-bit jiffy difference is wrap-safe
; for delays up to 255 seconds. Between stops the interpreter cycle calls
; bFTeleportTourTick; nothing runs when no tour is active.
.export _bFTeleportArrived, _bFTeleportTourTick

.segment "BANKRAM0F"
bFTourStop: .res 1
bFTourDelay: .res 2             ; jiffies
bFTourArrival: .res 2

; Begins at stop 0. A tour (two stops or more) waits _bFTeleportDelay
; seconds after each arrival.
bFTeleportTourBegin:
    lda _bFTeleportDelay        ; jiffies = seconds * 64 - seconds * 4
    stz bFTourDelay + 1
    asl a
    rol bFTourDelay + 1
    asl a
    rol bFTourDelay + 1
    sta tmp1                    ; seconds * 4
    ldx bFTourDelay + 1
    stx tmp2
    ldy #4
@times16:
    asl a
    rol bFTourDelay + 1
    dey
    bne @times16
    sec
    sbc tmp1
    sta bFTourDelay
    lda bFTourDelay + 1
    sbc tmp2
    sta bFTourDelay + 1
    lda _bFTeleportStopCount
    dec a
    sta _teleportTour           ; nonzero for two stops or more
    ldx #0
    ; Fall through.

; Requests stop X.
bFTeleportStartStop:
    stx bFTourStop
    lda _bFTeleportStopX,x
    sta _teleportX
    lda _bFTeleportStopY,x
    sta _teleportY
    lda _bFTeleportStopRoom,x
    sta _teleportPending
    rts

; void bFTeleportArrived(void): the current stop is done.
_bFTeleportArrived:
    ldx bFTourStop
    inx
    cpx _bFTeleportStopCount
    bcc @wait
    stz _teleportTour
    rts
@wait:
    jsr TELEPORT_RDTIM
    sta bFTourArrival
    stx bFTourArrival + 1
    rts

; void bFTeleportTourTick(void): a tour is waiting between stops.
_bFTeleportTourTick:
    jsr TELEPORT_RDTIM
    sec
    sbc bFTourArrival
    tay
    txa
    sbc bFTourArrival + 1
    cpy bFTourDelay
    sbc bFTourDelay + 1
    bcc @wait
    ldx bFTourStop
    inx
    bra bFTeleportStartStop
@wait:
    rts
.endif
