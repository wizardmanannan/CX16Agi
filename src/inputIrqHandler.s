; Render only complete foreground input publications; IRQ scratch is private.
.ifndef INPUTIRQ_INC
INPUTIRQ_INC = 1
MAX_INPUT_STRING_LENGTH = 40
.import _inputLineDisplayed, _user_input_line
.import _b7OutputString, _b7ReadingString, _b7InputReady
.import _b7InputRow, _b7InputColumn
.import _b3InputTextPalette
.include "globalGraphics.s"
.segment "BSS"
inputAddress: .res 2
inputColumn: .res 1
inputAvailable: .res 1
inputLength: .res 1
inputCount: .res 1
inputLastAddress: .res 2
inputLastLength: .res 1
.segment "BANKRAM07"
b7HandleInputLine:
    lda _b7InputReady
    bne :+
    rts
:   lda _b7ReadingString
    bne @string
    lda _inputLineDisplayed
    bne :+
    rts
:   lda _user_input_line+1
    beq :+
    rts
:   lda _user_input_line
    ldx #0
    bra @position
@string:
    lda _b7InputRow
    ldx _b7InputColumn
@position:
    cmp #25
    bcc :+
    rts
:   cpx #40
    bcc :+
    rts
:   stx inputColumn
    clc
    adc #FIRST_ROW-1
    lsr
    tax
    lda #0
    ror
    sta inputAddress
    txa
    clc
    adc #>MAP_BASE
    sta inputAddress+1
    lda inputColumn
    asl
    clc
    adc inputAddress
    sta inputAddress
    bcc :+
    inc inputAddress+1
:   lda #40
    sec
    sbc inputColumn
    sta inputAvailable
    ldy #0
@measure:
    cpy inputAvailable
    beq @measured
    lda _b7OutputString,y
    beq @measured
    iny
    bra @measure
@measured:
    sty inputLength
    sty inputCount
    lda inputAddress
    cmp inputLastAddress
    bne @draw
    lda inputAddress+1
    cmp inputLastAddress+1
    bne @draw
    lda inputLastLength
    cmp inputLength
    bcc @draw
    sta inputCount
@draw:
    stz VERA_ctrl
    lda #$10
    sta VERA_addr_bank
    lda inputAddress
    sta VERA_addr_low
    lda inputAddress+1
    sta VERA_addr_high
    ldy #0
    cpy inputCount
    beq @done
@cell:
    cpy inputLength
    bcs @erase
    lda _b7OutputString,y
    bra @write
@erase:
    lda #$20
@write:
    sta VERA_data0
    lda _b3InputTextPalette
    sta VERA_data0
    iny
    cpy inputCount
    bcc @cell
@done:
    lda inputAddress
    sta inputLastAddress
    lda inputAddress+1
    sta inputLastAddress+1
    lda inputLength
    sta inputLastLength
    rts
.endif
