; Resident bank selection around cc65's string routines. AX return values and
; the caller's bank survive; cross-bank copies retain a 50-byte stack buffer.
.importzp c_sp, ptr1, ptr2, ptr3, tmp1, tmp2, tmp3, tmp4
.import popax, incsp1, incsp6, addysp
.import _strlen, _strcpy, _memcpy, _memset
.export _strLenBanked, _strcpyBanked, _memCpyBanked, _memsetBanked
.export _memCpyBankedBetween
.export _strcmpIgnoreSpace
.segment "CODE"
_strLenBanked:
    tay
    lda $00
    pha
    sty $00
    jsr popax
    jsr _strlen
    bra bankedStringRestore
_strcpyBanked:
    tay
    lda $00
    pha
    sty $00
    jsr popax
    jsr _strcpy
    bra bankedStringRestore
_memsetBanked:
    tay
    lda $00
    pha
    sty $00
    jsr popax
    jsr _memset
    bra bankedStringRestore
_memCpyBanked:
    pha
    phx
    lda (c_sp)
    tax
    ldy $00
    stx $00
    jsr incsp1
    plx
    pla
    phy
    jsr _memcpy
bankedStringRestore:
    ply
    sty $00
    rts

; len in AX; stack arguments are srcBank/src/dstBank/dst (six bytes).
_memCpyBankedBetween:
    sta ptr3
    stx ptr3+1
    ora ptr3+1
    bne :+
    jmp incsp6
:
    sec
    lda c_sp
    sbc #50
    sta c_sp
    bcs :+
    dec c_sp+1
:
    lda $00
    sta tmp3
    ldy #50
    lda (c_sp),y
    sta tmp1
    iny
    lda (c_sp),y
    sta ptr1
    iny
    lda (c_sp),y
    sta ptr1+1
    iny
    lda (c_sp),y
    sta tmp2
    iny
    lda (c_sp),y
    sta ptr2
    iny
    lda (c_sp),y
    sta ptr2+1
bankedCopyChunk:
    lda #50
    ldx ptr3+1
    bne :+
    cmp ptr3
    bcc :+
    lda ptr3
:
    sta tmp4
    lda tmp1
    sta $00
    ldy tmp4
    dey
bankedCopyRead:
    lda (ptr1),y
    sta (c_sp),y
    dey
    bpl bankedCopyRead
    lda tmp2
    sta $00
    ldy tmp4
    dey
bankedCopyWrite:
    lda (c_sp),y
    sta (ptr2),y
    dey
    bpl bankedCopyWrite
    clc
    lda ptr1
    adc tmp4
    sta ptr1
    bcc :+
    inc ptr1+1
:
    clc
    lda ptr2
    adc tmp4
    sta ptr2
    bcc :+
    inc ptr2+1
:
    sec
    lda ptr3
    sbc tmp4
    sta ptr3
    bcs :+
    dec ptr3+1
:
    ora ptr3+1
    bne bankedCopyChunk
    lda tmp3
    sta $00
    ldy #56
    jmp addysp

; Dictionary comparison ignores ASCII spaces only. Preserve the C helper's
; +/-1 prefix results and unsigned-byte differences as a signed 16-bit int.
_strcmpIgnoreSpace:
    sta ptr2
    stx ptr2+1
    jsr popax
    sta ptr1
    stx ptr1+1
compareSkipLeft:
    lda (ptr1)
    cmp #32
    bne compareLeftReady
    inc ptr1
    bne compareSkipLeft
    inc ptr1+1
    bra compareSkipLeft
compareLeftReady:
    tax
compareSkipRight:
    lda (ptr2)
    cmp #32
    bne compareRightReady
    inc ptr2
    bne compareSkipRight
    inc ptr2+1
    bra compareSkipRight
compareRightReady:
    tay
    beq compareRightEnd
    cpx #0
    beq compareLeftEnd
    sta tmp1
    cpx tmp1
    bne compareDifference
    inc ptr1
    bne :+
    inc ptr1+1
:
    inc ptr2
    bne compareSkipLeft
    inc ptr2+1
    bra compareSkipLeft
compareDifference:
    txa
    sec
    sbc tmp1
    ldx #0
    bcs :+
    dex
:
    rts
compareRightEnd:
    cpx #0
    beq compareEqual
    lda #1
    ldx #0
    rts
compareLeftEnd:
    lda #$ff
    tax
    rts
compareEqual:
    lda #0
    tax
    rts
