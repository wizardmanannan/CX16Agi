; In-place console controls, ported from t3code-a0c2e42d's textControls.c.
; AX points to text in the currently mapped bank. Output never grows.
.importzp ptr1, ptr2
.export _b3ApplyTextModeControls
.segment "BSS"
controlRead: .res 2
controlLine: .res 2
controlCursor: .res 2
controlEnd: .res 2
.segment "CODE"
_b3ApplyTextModeControls:
    sta controlRead
    sta controlLine
    sta controlCursor
    sta controlEnd
    stx controlRead+1
    stx controlLine+1
    stx controlCursor+1
    stx controlEnd+1
controlLoop:
    lda controlRead
    sta ptr1
    lda controlRead+1
    sta ptr1+1
    lda (ptr1)
    bne :+
    jmp controlFinish
:
    inc controlRead
    bne :+
    inc controlRead+1
:   cmp #$0d
    beq controlReturn
    cmp #$08
    beq controlBackspace
    cmp #$0a
    beq controlLinefeed
    pha
    jsr controlPointCursor
    pla
    sta (ptr2)
    inc controlCursor
    bne :+
    inc controlCursor+1
:   lda controlCursor
    cmp controlEnd
    lda controlCursor+1
    sbc controlEnd+1
    bcc controlLoop
    lda controlCursor
    sta controlEnd
    lda controlCursor+1
    sta controlEnd+1
    jmp controlLoop
controlReturn:
    lda controlLine
    sta controlCursor
    lda controlLine+1
    sta controlCursor+1
    jmp controlLoop
controlBackspace:
    lda controlCursor
    cmp controlLine
    bne :+
    lda controlCursor+1
    cmp controlLine+1
    bne :+
    jmp controlLoop
:
:   lda controlCursor
    bne :+
    dec controlCursor+1
:   dec controlCursor
    jsr controlPointCursor
    lda #$20
    sta (ptr2)
    jmp controlLoop
controlLinefeed:
    lda controlEnd
    sta ptr2
    lda controlEnd+1
    sta ptr2+1
    lda #$0a
    sta (ptr2)
    inc controlEnd
    bne :+
    inc controlEnd+1
:   lda controlEnd
    ldx controlEnd+1
    sta controlLine
    sta controlCursor
    stx controlLine+1
    stx controlCursor+1
    jmp controlLoop
controlFinish:
    lda controlEnd
    sta ptr2
    lda controlEnd+1
    sta ptr2+1
    lda #0
    sta (ptr2)
    rts
controlPointCursor:
    lda controlCursor
    sta ptr2
    lda controlCursor+1
    sta ptr2+1
    rts
