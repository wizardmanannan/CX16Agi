.include "global.s"

CODE_WINDOW_SIZE = 10
codeWindow: .res CODE_WINDOW_SIZE
cwCurrentCode: .byte $0

refreshCodeWindow:
    jmp @start
    @previousBank: .byte $0

    tax

    lda RAM_BANK
    sta @previousBank

    stx RAM_BANK

    @start:
    ldy #$0

    @mainLoop:
    cpy #CODE_WINDOW_SIZE
    beq @endMainLoop

    lda (ZP_PTR_CODE),y
    sta codeWindow,y

    iny
    jmp @mainLoop
    @endMainLoop:

    stz cwCurrentCode

    lda @previousBank
    sta RAM_BANK
rts