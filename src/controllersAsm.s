.ifndef CONTROLLERS_INC

; Set the value of include guard and define constants
CONTROLLERS_INC = 1

.import _b1ControllerBits

.segment "BANKRAM01"
b1IsControllerSet:              ; controller in X, returns 0/1 in A
        tax
        lsr
        lsr
        lsr
        tay                     ; Y = byte index

        lda _b1ControllerBits,y
        sta sreg

        txa
        and #7
        tax                     ; X = bit position

        lda #1
@shift:
        cpx #0
        beq @test
        asl
        dex
        bra @shift

@test:
        and sreg
        bne @one

        lda #0
        rts
@one:
        lda #1
        rts

.endif