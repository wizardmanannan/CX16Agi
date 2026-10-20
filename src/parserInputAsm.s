; Punctuation compaction reused from t3code-a0c2e42d; tokenization and input
; composition share this resident module. No second C implementation.
.ifndef PARSER_INPUT_ASM_INC
PARSER_INPUT_ASM_INC = 1
.importzp ptr1, ptr2, tmp1, tmp2
.import popax
.export _b7StripExtraChars, _b7StripExtraCharsEnd
.export _b7TokenizeWords
.export _b7GetInputWord
.import _inputWords
.export _b7RefreshInputLine
.import _b7Temp, _b7CurrentInputStr, _b7OutputString, _cursorChar
.segment "CODE"
; Dictionary synonym IDs are words, including values above 255.
_b7GetInputWord:
    asl
    tay
    lda _inputWords+1,y
    tax
    lda _inputWords,y
    rts

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

; Tokens table in AX, text on the software stack. Like the original strtok
; loop, replace only each token's first following space and return the last
; table slot (base-2 when empty). The parser supplies room for 20 tokens.
_b7TokenizeWords:
    sta ptr2
    stx ptr2+1
    jsr popax
    sta ptr1
    stx ptr1+1
@skip:
    lda (ptr1)
    beq @done
    cmp #32
    bne @token
    inc ptr1
    bne @skip
    inc ptr1+1
    bra @skip
@token:
    lda ptr1
    sta (ptr2)
    ldy #1
    lda ptr1+1
    sta (ptr2),y
    clc
    lda ptr2
    adc #2
    sta ptr2
    bcc @word
    inc ptr2+1
@word:
    inc ptr1
    bne :+
    inc ptr1+1
:
    lda (ptr1)
    beq @done
    cmp #32
    bne @word
    lda #0
    sta (ptr1)
    inc ptr1
    bne @skip
    inc ptr1+1
    bra @skip
@done:
    sec
    lda ptr2
    sbc #2
    tay
    lda ptr2+1
    sbc #0
    tax
    tya
    rts

; Compose prompt + input + cursor, optionally followed by a blank to erase the
; previous cursor cell. Reserve byte 79 for NUL; parser.c checks these sizes.
_b7RefreshInputLine:
    sta tmp2
    ldx #0
    ldy #0
@prompt:
    lda _b7Temp,y
    beq @inputStart
    sta _b7OutputString,x
    inx
    cpx #79
    beq @finish
    iny
    bra @prompt
@inputStart:
    ldy #0
@input:
    lda _b7CurrentInputStr,y
    beq @cursor
    sta _b7OutputString,x
    inx
    cpx #79
    beq @finish
    iny
    bra @input
@cursor:
    lda _cursorChar
    sta _b7OutputString,x
    inx
    cpx #79
    beq @finish
    lda tmp2
    beq @finish
    lda #32
    sta _b7OutputString,x
    inx
@finish:
    lda #0
    sta _b7OutputString,x
    rts
.endif
