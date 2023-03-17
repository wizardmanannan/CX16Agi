.ifndef  CODE_WINDOW_INC
CODE_WINDOW_INC = 1
.include "global.s"
.ifdef DEBUG
.import _debugPrintCurrentCodeState
.endif


ZP_PTR_CODE_WIN = $08

CODE_WINDOW_SIZE = 10
.segment "CODE"
codeWindow: .res CODE_WINDOW_SIZE
cwCurrentCode: .word $0
codeWindowAddress: .addr codeWindow
codeWindowInvalid: .byte TRUE
codeBankArray: .byte $5,$1,$1,$1,$1,$1,$1,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$2,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$1,$1,$3,$3,$3,$3,$3,$3,$3,$3,$3,$3,$4,$4,$4,$4,$4,$4,$4,$4,$1,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$4,$1,$1,$1,$4,$4,$4,$4,$4,$4,$4,$1,$4,$1,$1,$1,$1,$4,$1,$1,$1,$4,$4,$4,$4,$1,$1,$4,$4,$4,$4,$1,$4,$5,$1,$1,$1,$5,$5,$1,$1,$5,$5,$5,$5,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1,$1

.macro DEBUG_CODE_STATE
.ifdef DEBUG
 JSRFAR b5DebugCodeState, DEBUG_BANK
.endif
.endmacro

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
cmp #CODE_WINDOW_SIZE - 1
bne @end
stz cwCurrentCode
jsr refreshCodeWindow
@end:
.endmacro

.macro INC_CODE_BY jumpAmount
.local @start
.local @end

ADD_WORD_16 cwCurrentCode, jumpAmount, cwCurrentCode
lda cwCurrentCode
cmp #CODE_WINDOW_SIZE - 1
bcc @end
CATCH_UP_CODE
jsr refreshCodeWindow
@end:
.endmacro

.macro CATCH_UP_CODE ;Warning Invalidates The Code Window Call Refresh Afterwards
ADD_WORD_16_8 ZP_PTR_CODE, cwCurrentCode, ZP_PTR_CODE

lda #TRUE
sta codeWindowInvalid
.endmacro

.SEGMENT "CODE"
refreshCodeWindow:
    bra @start
    @previousBank: .byte $0
    @codeIncrement: .word CODE_WINDOW_SIZE - 1
    @start:
    lda codeWindowInvalid
    cmp #TRUE
    beq @storeAndSetBank

    ADD_WORD_16 ZP_PTR_CODE, @codeIncrement, ZP_PTR_CODE

    @storeAndSetBank:
    lda RAM_BANK
    sta @previousBank

    lda _codeBank
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
    stz cwCurrentCode + 1

    lda #FALSE
    sta codeWindowInvalid

    lda @previousBank
    sta RAM_BANK
rts

_loadAndIncWinCode:
    bra @start
    @result: .byte $0
    @start:
    LOAD_CODE_WIN_CODE
    sta @result
    
    INC_CODE
    
    lda @result
    ldx #$0
    rts

_incCodeBy:
    bra @start
    @jumpAmount: .word $0
    @start:
    sta @jumpAmount
    stx @jumpAmount + 1

    INC_CODE_BY @jumpAmount
    rts

.SEGMENT "BANKRAM07"
b7CodeWindowInit:
lda codeWindowAddress
sta ZP_PTR_CODE_WIN

lda codeWindowAddress + 1
sta ZP_PTR_CODE_WIN + 1
rts

.SEGMENT "BANKRAM05"
.ifdef DEBUG
b5DebugCodeState:
    bra @start
    @result: .word $0
    @start:
    clc
    lda ZP_PTR_CODE
    adc cwCurrentCode
    sta @result

    ldx ZP_PTR_CODE + 1
    adc #$0
    stx @result + 1


    jsr _debugPrintCurrentCodeState
    rts
.endif

.endif