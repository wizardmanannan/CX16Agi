; In-place greedy wrapping and bounded dialogue borders, current bank unchanged.
; Buffers are 1000 bytes each; at most 20 rows (including four frame rows).
.importzp ptr1, ptr2, c_sp
.import incsp4, incsp1
.import _b3TextBuffers, _b3LastBoxLines
.export _b3WrapText, _b3DrawBorder
.segment "BSS"
wrapMaximum: .res 2
wrapWidth: .res 2
wrapSpace: .res 2
wrapLines: .res 1
layoutWidth: .res 1
layoutRowBytes: .res 1
layoutRows: .res 1
layoutMore: .res 1
layoutCapacity: .res 2
.segment "CODE"
; maxWidth in AX, width and input pointer on cc65's software stack.
_b3WrapText:
    sta wrapMaximum
    stx wrapMaximum+1
    sta ptr2
    stx ptr2+1
    lda #0
    sta (ptr2)
    sta wrapSpace
    sta wrapSpace+1
    inc a
    sta wrapLines
    ldy #0
    lda (c_sp),y
    sta wrapWidth
    iny
    lda (c_sp),y
    sta wrapWidth+1
    iny
    lda (c_sp),y
    sta ptr1
    sta ptr2
    iny
    lda (c_sp),y
    sta ptr1+1
    sta ptr2+1
wrapLoop:
    lda (ptr1)
    bne :+
    lda wrapLines
    ldx #0
    jmp incsp4
:
    cmp #10
    beq wrapHardBreak
    cmp #32
    bne :+
    lda ptr1
    sta wrapSpace
    lda ptr1+1
    sta wrapSpace+1
:
    lda wrapSpace
    ora wrapSpace+1
    beq wrapAdvance
    bit wrapWidth+1
    bmi wrapSoftBreak
    sec
    lda ptr1
    sbc ptr2
    tax
    lda ptr1+1
    sbc ptr2+1
    cmp wrapWidth+1
    bcc wrapAdvance
    bne wrapSoftBreak
    cpx wrapWidth
    bcc wrapAdvance
    beq wrapAdvance
wrapSoftBreak:
    ; Compare the full distance before retaining the original byte maxWidth.
    sec
    lda wrapSpace
    sbc ptr2
    tax
    lda wrapSpace+1
    sbc ptr2+1
    tay
    inx
    bne :+
    iny
:
    lda wrapMaximum
    sta ptr2
    lda wrapMaximum+1
    sta ptr2+1
    tya
    bne wrapSetMaximum
    txa
    cmp (ptr2)
    bcc :+
wrapSetMaximum:
    txa
    sta (ptr2)
:
    lda wrapSpace
    sta ptr2
    lda wrapSpace+1
    sta ptr2+1
    lda #10
    sta (ptr2)
    bra wrapNewLine
wrapHardBreak:
    lda ptr1
    sta ptr2
    lda ptr1+1
    sta ptr2+1
wrapNewLine:
    inc ptr2
    bne :+
    inc ptr2+1
:
    stz wrapSpace
    stz wrapSpace+1
    inc wrapLines
    inc _b3LastBoxLines
wrapAdvance:
    inc ptr1
    bne :+
    inc ptr1+1
:
    jmp wrapLoop

; messageSize in AX is intentionally unused; width is the stacked byte.
_b3DrawBorder:
    lda (c_sp)
    cmp #2
    bcs :+
    lda #2
:
    cmp #63
    bcc :+
    lda #62
:
    sta layoutWidth
    inc a
    inc a
    sta layoutRowBytes
    lda #<1000
    sta layoutCapacity
    lda #>1000
    sta layoutCapacity+1
    ldx #0
borderCapacity:
    sec
    lda layoutCapacity
    sbc layoutRowBytes
    tay
    lda layoutCapacity+1
    sbc #0
    bcc borderCapacityDone
    sta layoutCapacity+1
    sty layoutCapacity
    inx
    cpx #20
    bne borderCapacity
borderCapacityDone:
    txa
    sec
    sbc #4
    sta layoutRows
    lda #4
    sta _b3LastBoxLines
    lda #<_b3TextBuffers
    sta ptr1
    lda #>_b3TextBuffers
    sta ptr1+1
    lda #<(_b3TextBuffers+1000)
    sta ptr2
    lda #>(_b3TextBuffers+1000)
    sta ptr2+1
    lda #158
    jsr borderHorizontal
    jsr borderPadding
borderContent:
    lda #159
    sta (ptr2)
    ldy #1
    stz layoutMore
borderRead:
    lda (ptr1)
    beq borderPad
    inc ptr1
    bne :+
    inc ptr1+1
:
    cmp #10
    bne :+
    inc layoutMore
    bra borderPad
:
    cpy layoutWidth
    bcs borderRead
    sta (ptr2),y
    iny
    bra borderRead
borderPad:
    lda #32
:
    cpy layoutWidth
    bcs :+
    sta (ptr2),y
    iny
    bra :-
:
    jsr borderRowEnd
    inc _b3LastBoxLines
    dec layoutRows
    beq borderFinish
    lda layoutMore
    bne borderContent
borderFinish:
    jsr borderPadding
    ; The last row ends in NUL rather than LF.
    lda #157
    jsr borderHorizontal
    lda ptr2
    bne :+
    dec ptr2+1
:
    dec ptr2
    lda #0
    sta (ptr2)
    jmp incsp1
borderPadding:
    lda #159
    sta (ptr2)
    ldy #1
    bra borderPadOnly
borderPadOnly:
    lda #32
:
    sta (ptr2),y
    iny
    cpy layoutWidth
    bcc :-
borderRowEnd:
    lda #156
    sta (ptr2),y
    iny
    lda #10
    sta (ptr2),y
    bra borderAdvance
borderHorizontal:
    ldy #0
:
    sta (ptr2),y
    iny
    cpy layoutWidth
    bcc :-
    beq :-
    lda #10
    sta (ptr2),y
borderAdvance:
    clc
    lda ptr2
    adc layoutRowBytes
    sta ptr2
    bcc :+
    inc ptr2+1
:
    rts
