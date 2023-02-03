.ifndef  CODE_WINDOW_INC
CODE_WINDOW_INC = 1
.include "global.s"

ZP_PTR_CODE_WIN = $08

CODE_WINDOW_SIZE = 10

.segment "CODE"
CODE_WINDOW_NOT_INITED_BYTE = $FF
codeWindow: .res CODE_WINDOW_SIZE, CODE_WINDOW_NOT_INITED_BYTE
cwCurrentCode: .byte $0

codeWindowAddress: .addr codeWindow

codeWindowInit:
lda codeWindowAddress
sta ZP_PTR_CODE_WIN

lda codeWindowAddress + 1
sta ZP_PTR_CODE_WIN + 1
rts


.macro LOAD_CODE_WIN_CODE
        ldy cwCurrentCode
        lda (ZP_PTR_CODE_WIN),y
.endmacro

.macro INC_CODE
.local @start
.local @end

inc cwCurrentCode
lda cwCurrentCode
cmp CODE_WINDOW_SIZE
bne @end

stz cwCurrentCode
jsr refreshCodeWindow
@end:
.endmacro


refreshCodeWindow:
    bra @start
    @previousBank: .byte $0
    @codeIncrement: .word CODE_WINDOW_SIZE + 1
    @start:

    lda codeWindow
    cmp #CODE_WINDOW_NOT_INITED_BYTE
    beq @storeAndSetBank

    ADD_WORD_16 ZP_PTR_CODE, @codeIncrement, ZP_PTR_CODE

    @storeAndSetBank:
    lda RAM_BANK
    sta @previousBank

    lda codeBank
    sta RAM_BANK

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
.endif