.ifndef  CODE_WINDOW_INC
CODE_WINDOW_INC = 1
.include "global.s"

ZP_PTR_CODE_WIN = $08

CODE_WINDOW_SIZE = 10

.segment "CODE"
codeWindow: .res CODE_WINDOW_SIZE
cwCurrentCode: .byte $0

codeWindowAddress: .addr codeWindow

codeWindowInit:
lda codeWindowAddress
sta ZP_PTR_CODE_WIN

lda codeWindowAddress + 1
sta ZP_PTR_CODE_WIN + 1
rts

.macro INC_CODE
.local @start
.local @end
.local @codeIncrement

bra @start

@codeIncrement: .word $1

@start:
ADD_WORD_16 ZP_PTR_CODE, @codeIncrement, ZP_PTR_CODE

inc cwCurrentCode
lda cwCurrentCode
cmp CODE_WINDOW_SIZE
bne @end

stz cwCurrentCode
jsr refreshCodeWindow
@end:
.endmacro


refreshCodeWindow:
    jmp @start
    @previousBank: .byte $0

    @start:
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