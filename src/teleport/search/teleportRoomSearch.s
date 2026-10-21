.ifndef TELEPORT_ROOM_SEARCH_INC
TELEPORT_ROOM_SEARCH_INC = 1
; teleportRoomSearch.s - Which room best matches each stop's name?
;
; AGI games have no room-name table, so a name is matched against the text a
; room's script can print: "moat" finds the logic whose messages mention the
; moat most. Rooms are logics with a picture of the same number, which skips
; most helper scripts. Each room's message text is streamed from its VOL file
; through the scratch block - no heap, no resource loading - so every stop is
; resolved in a single pass over the game. Score: distinct words found, then
; total matches; ties keep the lower room.
.importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3, sreg
.import pusha, _numLogics
.import _b6Cbm_openForSeeking, _b6Cx16_fseek

TELEPORT_CHANNEL = 2            ; FILE_OPEN_ADDRESS

.segment "BANKRAM0F"
bFSearchRoom: .res 1
bFSearchVolume: .res 1          ; open VOL file, $FF if none
bFSearchPass: .res 1            ; volume scanned in this pass
bFSearchNext: .res 1            ; lowest later volume seen, $FF if none
bFSearchOffset: .res 3          ; file position
bFSearchLeft: .res 2            ; message text still to scan
bFSearchCount: .res 1           ; bytes in this read
bFSearchBestLow: .res TELEPORT_MAX_STOPS
bFSearchBestHigh: .res TELEPORT_MAX_STOPS

; Sets bFTeleportStopRoom for every named stop: 0 if nothing matched. One
; pass per VOL file, lowest first, so each file is opened once; each pass also
; finds the next volume to visit.
bFTeleportFindRooms:
    ldx #TELEPORT_MAX_STOPS - 1
@clear:
    stz bFSearchBestLow,x
    stz bFSearchBestHigh,x
    dex
    bpl @clear
    lda #$FF
    sta bFSearchVolume
    stz bFSearchPass
@pass:
    lda #$FF
    sta bFSearchNext
    lda #1
    sta bFSearchRoom
@room:
    lda _numLogics + 1
    bne @inRange
    lda bFSearchRoom
    cmp _numLogics
    bcs @passed
@inRange:
    lda bFSearchRoom
    ldx #0
    jsr bFTeleportDirectory
    bcs @next
    lda (ptr3)                  ; volume
    cmp bFSearchPass
    beq @thisPass
    bcc @next                   ; an earlier pass did it
    cmp bFSearchNext
    bcs @next
    sta bFSearchNext
    bra @next
@thisPass:
    ldy #3
@position:
    lda (ptr3),y
    sta bFSearchOffset - 1,y
    dey
    bne @position
    lda bFSearchRoom
    ldx #1
    jsr bFTeleportDirectory     ; rooms have pictures
    bcs @next
    jsr bFSearchRoomText
    jsr bFSearchScore
@next:
    inc bFSearchRoom
    bne @room
@passed:
    lda bFSearchNext
    sta bFSearchPass
    inc a
    bne @pass
    lda bFSearchVolume
    bmi @closed
    lda #TELEPORT_LFN
    jmp TELEPORT_CLOSE
@closed:
    rts

bFSearchAbort:
    rts

; Scans the message text of the logic at bFSearchOffset in this pass's volume.
bFSearchRoomText:
    lda bFSearchPass
    cmp bFSearchVolume
    beq @opened
    pha
    ldx bFSearchVolume
    bmi @open
    lda #TELEPORT_LFN
    jsr TELEPORT_CLOSE
@open:
    pla
    sta bFSearchVolume
    jsr bFSearchOpen
@opened:
    lda #7                      ; 12 34 volume size(2) code-size(2)
    jsr bFSearchReadAt
    bne bFSearchAbort
    lda (ptr3)
    cmp #$12
    bne bFSearchAbort
    ldy #1
    lda (ptr3),y
    cmp #$34
    bne bFSearchAbort
    ldy #3                      ; left = size - code size
    lda (ptr3),y
    ldy #5
    sec
    sbc (ptr3),y
    sta bFSearchLeft
    ldy #4
    lda (ptr3),y
    ldy #6
    sbc (ptr3),y
    sta bFSearchLeft + 1
    lda #7                      ; offset += 7 + code size: the message section
    dey
    clc
    adc (ptr3),y
    tax
    iny
    lda (ptr3),y
    adc #0
    jsr bFSearchAdvance
    lda #3                      ; message count, end offset(2)
    jsr bFSearchReadAt
    bne @done
    lda (ptr3)                  ; 3 + 2 * count bytes precede the text
    asl a
    tax
    lda #0
    rol a
    tay
    txa
    clc
    adc #3
    tax
    tya
    adc #0
    jsr bFSearchAdvance
    txa                         ; left -= 2 (size field) + 3 + 2 * count
    clc
    adc #2
    sta tmp1
    tya
    adc #0
    sta tmp2
    lda bFSearchLeft
    sec
    sbc tmp1
    sta bFSearchLeft
    lda bFSearchLeft + 1
    sbc tmp2
    sta bFSearchLeft + 1
    bcc @done                   ; malformed: no text
    jsr bFSearchSeek
    jsr bFTeleportScanReset
@chunk:
    lda bFSearchLeft + 1
    bne @full
    lda bFSearchLeft
    beq @end
    cmp #TELEPORT_SCRATCH_SIZE
    bcc @read
@full:
    lda #TELEPORT_SCRATCH_SIZE
@read:
    jsr bFSearchRead
    bne @end                    ; short read: scan no further
    lda ptr3                    ; scratch, as data
    sta ptr1
    lda ptr3 + 1
    sta ptr1 + 1
    lda bFSearchCount
    jsr bFTeleportScan
    lda bFSearchLeft
    sec
    sbc bFSearchCount
    sta bFSearchLeft
    bcs @chunk
    dec bFSearchLeft + 1
    bra @chunk
@end:
    jsr bFTeleportScanEnd
@done:
    rts

; Scores this room for every named stop from the scan's hit counts.
bFSearchScore:
    ldx #0
@stop:
    cpx _bFTeleportStopCount
    bcs @done
    stz tmp1                    ; distinct words
    stz tmp2                    ; total, low
    stz tmp3                    ; total, high
    ldy _bFTeleportStopFirst,x
@word:
    tya
    cmp _bFTeleportStopFirst + 1,x
    bcs @score
    lda _bFTeleportHits,y
    beq @nextWord
    inc tmp1
    clc
    adc tmp2
    sta tmp2
    bcc @nextWord
    inc tmp3
@nextWord:
    iny
    bra @word
@score:
    lda tmp1                    ; score = distinct * 4096 + total
    beq @nextStop
    asl a
    asl a
    asl a
    asl a
    ora tmp3
    sta tmp3
    lda bFSearchBestHigh,x      ; better, or as good and a lower room
    cmp tmp3
    bne @decided
    lda bFSearchBestLow,x
    cmp tmp2
    bne @decided
    lda bFSearchRoom
    cmp _bFTeleportStopRoom,x
@decided:
    bcs @nextStop
    lda tmp2
    sta bFSearchBestLow,x
    lda tmp3
    sta bFSearchBestHigh,x
    lda bFSearchRoom
    sta _bFTeleportStopRoom,x
@nextStop:
    inx
    bra @stop
@done:
    rts

; Opens VOL.<A> ("vol.N" in PETSCII, as agifiles.c opens it) for seeking.
bFSearchOpen:
    tax
    jsr bFTeleportScratchPointer
    ldy #3
@name:
    lda bFSearchVol,y
    sta (ptr3),y
    dey
    bpl @name
    ldy #4
    txa
    cmp #10
    bcc @units
    sbc #10
    tax
    lda #$31                    ; "1"
    sta (ptr3),y
    iny
    txa
@units:
    ora #$30
    sta (ptr3),y
    iny
    lda #0
    sta (ptr3),y
    lda ptr3
    ldx ptr3 + 1
    TRAMPOLINE #TELEPORT_ROOM_BANK, _b6Cbm_openForSeeking
    rts
bFSearchVol: .byte $56, $4F, $4C, $2E

; offset += X + A * 256. Preserves X and Y.
bFSearchAdvance:
    pha
    txa
    clc
    adc bFSearchOffset
    sta bFSearchOffset
    pla
    adc bFSearchOffset + 1
    sta bFSearchOffset + 1
    bcc @done
    inc bFSearchOffset + 2
@done:
    rts

; Seeks to the offset, then reads A bytes. Out: Z set if all were read.
bFSearchReadAt:
    pha
    jsr bFSearchSeek
    pla
    ; Fall through.

; Reads A (1-128) bytes into scratch with the KERNAL block read, byte by byte
; where the device has none. Out: Z set if all were read; ptr3 = scratch.
bFSearchRead:
    sta bFSearchCount
    sta tmp1                    ; still to read; the KERNAL keeps off cc65's ZP
    jsr bFTeleportScratchPointer
    lda ptr3
    sta ptr2
    lda ptr3 + 1
    sta ptr2 + 1
    ldx #TELEPORT_LFN
    jsr TELEPORT_CHKIN
@block:
    lda tmp1
    ldx ptr2
    ldy ptr2 + 1
    clc                         ; advance the destination
    jsr TELEPORT_MACPTR
    bcs @bytes
    txa                         ; read at most A < 256
    beq @done                   ; end of file
    adc ptr2                    ; carry is clear
    sta ptr2
    bcc @counted
    inc ptr2 + 1
@counted:
    stx tmp2
    lda tmp1
    sec
    sbc tmp2
    sta tmp1
    bne @block
    bra @done
@bytes:
    jsr TELEPORT_BASIN
    sta (ptr2)
    inc ptr2
    bne @stored
    inc ptr2 + 1
@stored:
    dec tmp1
    beq @done
    jsr TELEPORT_READST
    beq @bytes                  ; no end of file, no error
@done:
    jsr TELEPORT_CLRCHN
    lda tmp1
    rts

; Seeks the open VOL file to the offset.
bFSearchSeek:
    lda #TELEPORT_CHANNEL
    jsr pusha
    lda bFSearchOffset + 2
    sta sreg
    stz sreg + 1
    lda bFSearchOffset
    ldx bFSearchOffset + 1
    TRAMPOLINE #TELEPORT_ROOM_BANK, _b6Cx16_fseek
    rts
.endif
