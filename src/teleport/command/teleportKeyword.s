.ifndef TELEPORT_KEYWORD_INC
TELEPORT_KEYWORD_INC = 1
; teleportKeyword.s - Is this input line a teleport command?
;
; It is when it starts with "teleport" or "tp" followed by the end of the
; line, a space or a colon. While such a line is being typed, keys go to the
; line instead of the game's controller bindings (KQ1 binds 0 to Jump), and
; ':' and ',' are accepted.
.importzp ptr1
.export _b7TeleportInputPrefix

.segment "BANKRAM07"
TELEPORT_ASCII

; In: ptr1 = line. Out: A = keyword length (2 or 8), or 0. Z reflects A.
b7TeleportKeyword:
    lda (ptr1)
    cmp #'t'
    bne @no
    ldy #1
    lda (ptr1),y
    cmp #'p'
    beq @short
@long:
    lda (ptr1),y
    cmp b7TeleportTail - 1,y
    bne @no
    iny
    cpy #8
    bne @long
    .byte $2C                   ; bit abs: skip the ldy
@short:
    ldy #2
    lda (ptr1),y
    beq @yes
    cmp #' '
    beq @yes
    cmp #':'
    bne @no
@yes:
    tya
    rts
@no:
    lda #0
    rts

b7TeleportTail: .byte "eleport"

; boolean b7TeleportInputPrefix(const char* input)
_b7TeleportInputPrefix:
    sta ptr1
    stx ptr1 + 1
    jsr b7TeleportKeyword
    ldx #0
    rts
.popcharmap
.endif
