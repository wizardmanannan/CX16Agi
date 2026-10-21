.ifndef TELEPORT_TEXT_INC
TELEPORT_TEXT_INC = 1
; teleportText.s - How is a reply composed?
;
; Appends ASCII to a zero-terminated message in the command's scratch block
; and shows it with bank 7's message box. Nothing here calls C, so ptr3
; stays valid between appends.
.importzp ptr1, ptr2, ptr3, tmp1, tmp2, tmp3

.segment "BANKRAM0F"
TELEPORT_ASCII
TELEPORT_TEXT_SIZE = 120        ; within the scratch block

bFTextLength: .res 1

; Starts an empty message with the bank-F string at A/X.
bFTextBegin:
    stz bFTextLength
    jsr bFTeleportScratchPointer
    ; Fall through.

; Appends the zero-terminated bank-F string at A/X.
bFTextString:
    sta ptr2
    stx ptr2 + 1
    ldy #0
@char:
    lda (ptr2),y
    beq @done
    jsr bFTextChar
    iny
    bne @char
@done:
    rts

; Appends A. Preserves X and Y.
bFTextChar:
    phy
    ldy bFTextLength
    cpy #TELEPORT_TEXT_SIZE - 1
    bcs @full
    sta (ptr3),y
    iny
    sty bFTextLength
@full:
    lda #0
    sta (ptr3),y
    ply
    rts

; Appends A (0-255) in decimal, without leading zeros.
bFTextNumber:
    ldx #2
    stz tmp3                    ; bit 7: a digit has been written
@place:
    ldy #'0' - 1
@count:
    iny
    sec
    sbc bFTextPowers - 1,x
    bcs @count
    adc bFTextPowers - 1,x      ; carry is clear: undo the last subtraction
    pha
    tya
    cpy #'0'
    bne @write
    bit tmp3
    bpl @skip
@write:
    jsr bFTextChar
    dec tmp3
@skip:
    pla
    dex
    bne @place
    ora #'0'
    jmp bFTextChar
bFTextPowers: .byte 10, 100

; Appends stop X's name as typed: from its first word to the end of its last.
bFTextName:
    jsr bFTeleportLinePointer
    ldy _bFTeleportStopFirst + 1,x
    lda _bFTeleportWordStart - 1,y
    clc
    adc _bFTeleportWordLength - 1,y
    sta tmp1                    ; end
    ldy _bFTeleportStopFirst,x
    lda _bFTeleportWordStart,y
    tay
@char:
    lda (ptr1),y
    jsr bFTextChar
    iny
    cpy tmp1
    bne @char
    rts

; Shows the message at A/X from bank 0xF. Carry set: wait for Enter/Escape.
bFTeleportShow:
    stz tmp2
    ror tmp2
    ldy #TELEPORT_BANK
    sty tmp1
    TELEPORT_JUMP #TELEPORT_NOTICE_BANK, b7TeleportShow

; Shows the message being built, waiting for Enter/Escape.
bFTextShow:
    lda bFTeleportScratch
    ldx bFTeleportScratch + 1
    sec
    bra bFTeleportShow
.popcharmap
.endif
