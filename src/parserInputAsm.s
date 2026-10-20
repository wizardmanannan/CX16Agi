; Reused from t3code-a0c2e42d; no second C implementation.
.ifndef PARSER_INPUT_ASM_INC
PARSER_INPUT_ASM_INC = 1
.importzp ptr1, ptr2, tmp1, tmp2
.export _b7StripExtraChars, _b7StripExtraCharsEnd
.segment "CODE"
; Resident code also permits the emulator fixture to supply another input bank.

; Read and write cursors permit compaction in place, including page crossings.
; 65C02 (zp) addressing avoids reloading Y for each input/output byte.
; The bitset classifies ASCII punctuation in constant time; high bytes survive.
_b7StripExtraChars:
    sta ptr1
    sta ptr2
    stx ptr1+1
    stx ptr2+1
    stz tmp2
@read:
    lda (ptr1)
    beq @end
    inc ptr1
    bne :+
    inc ptr1+1
:
    sta tmp1
    tax
    bmi @keep
    lsr
    lsr
    lsr
    tax
    lda tmp1
    and #7
    tay
    lda @bits,y
    and @punctuation,x
    bne @read
@keep:
    lda tmp1
    cmp #$20
    bne @write
    cmp tmp2
    beq @read
@write:
    sta tmp2
    sta (ptr2)
    inc ptr2
    bne @read
    inc ptr2+1
    bra @read
@end:
    sta (ptr2)
    rts
@bits:
    .byte 1,2,4,8,16,32,64,128
@punctuation:
    .byte $00,$00,$00,$00,$86,$73,$00,$8C,$00,$00,$00,$28,$01,$00,$00,$28
_b7StripExtraCharsEnd:
.endif
