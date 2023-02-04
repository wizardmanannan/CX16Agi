.ifndef  CODE_WINDOW_INC
CODE_WINDOW_INC = 1
.include "global.s"

ZP_PTR_CODE_WIN = $08

CODE_WINDOW_SIZE = 10

.segment "CODE"
codeWindow: .res CODE_WINDOW_SIZE
cwCurrentCode: .byte $0
codeWindowAddress: .addr codeWindow
codeWindowInvalid: .byte TRUE

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

.macro CATCH_UP_CODE ;Warning Invalidates The Code Window Call Refresh Afterwards
ADD_WORD_16_8 ZP_PTR_CODE, cwCurrentCode, ZP_PTR_CODE

lda #TRUE
sta codeWindowInvalid
.endmacro

refreshCodeWindow:
    bra @start
    @previousBank: .byte $0
    @codeIncrement: .word CODE_WINDOW_SIZE + 1
    @start:

    lda codeWindowInvalid
    cmp #TRUE
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

_loadAndIncWinCode:

    LOAD_CODE_WIN_CODE
    tax

    INC_CODE
    
    txa
    rts
.endif