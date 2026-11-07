; In-place console controls, ported from t3code-a0c2e42d's textControls.c.
; AX points to text in the currently mapped bank. Output never grows.
.importzp ptr1, ptr2
.export _b3ApplyTextModeControls
.export _b3ClipTextToMap, _textClipColumns, _textClipRows
.segment "BSS"
controlRead: .res 2
controlLine: .res 2
controlCursor: .res 2
controlEnd: .res 2
_textClipColumns: .res 1
_textClipRows: .res 1
clipColumnsLeft: .res 1
clipLineBase: .res 1
clipPhysicalLine: .res 1
clipLines: .res 1
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

; AX points to the display buffer. Preserve VERA's sequential row wrapping,
; but discard cells outside the map. Explicit LF advances from the line origin.
_b3ClipTextToMap:
    sta ptr1
    sta ptr2
    stx ptr1+1
    stx ptr2+1
    stz clipLines
    lda _textClipColumns
    beq clipFinish
    sta clipColumnsLeft
    lda _textClipRows
    beq clipFinish
    lda #1
    sta clipLines
    sta clipLineBase
    sta clipPhysicalLine
clipLoop:
    lda (ptr1)
    beq clipFinish
    inc ptr1
    bne :+
    inc ptr1+1
:   cmp #$0a
    beq clipNewline
    ldx clipColumnsLeft
    bne clipStore
    ldx clipPhysicalLine
    cpx _textClipRows
    bcs clipLoop
    inc clipPhysicalLine
    pha
    lda clipPhysicalLine
    jsr clipMaxLine
    lda #64
    sta clipColumnsLeft
    pla
clipStore:
    dec clipColumnsLeft
    sta (ptr2)
    jsr clipAdvance
    bra clipLoop
clipNewline:
    ldx clipLineBase
    cpx _textClipRows
    bcs clipFinish
    sta (ptr2)
    jsr clipAdvance
    inc clipLineBase
    lda clipLineBase
    sta clipPhysicalLine
    jsr clipMaxLine
    lda _textClipColumns
    sta clipColumnsLeft
    bra clipLoop
clipFinish:
    lda #0
    sta (ptr2)
    lda clipLines
    ldx #0
    rts
clipAdvance:
    inc ptr2
    bne :+
    inc ptr2+1
:   rts
clipMaxLine:
    cmp clipLines
    bcc :+
    sta clipLines
:   rts
