; This code deals with managing a code window that stores op codes.
; It helps to access op codes from different banks and allows execution of scripts.
; The code window can hold CODE_WINDOW_SIZE op codes and has a counter cwCurrentCode to track the next op code to process.

.ifndef  CODE_WINDOW_INC
CODE_WINDOW_INC = 1
.include "global.s"

; Import debug functions if in DEBUG mode.
.ifdef DEBUG
.import _debugPrintCurrentCodeState
.import _stopAtFunc 
.endif

CODE_WINDOW_SIZE = 50
.segment "CODE"
codeWindow: .res CODE_WINDOW_SIZE 
cwCurrentCode: .word $0 ;A counter 
codeWindowAddress: .addr codeWindow
codeWindowInvalid: .byte TRUE

; Macro for loading code from the window without incrementing.
.macro LOAD_CODE_WIN_CODE
        ldy cwCurrentCode
        lda codeWindow,y
.endmacro

; Macro for incrementing code and refreshing the code window if necessary.
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

; Macro for incrementing code by a given amount and refreshing the code window if necessary.
.macro INC_CODE_BY jumpAmount
.local @start
.local @end
.local @catchUp
ADD_WORD_16 cwCurrentCode, jumpAmount, cwCurrentCode
lda cwCurrentCode + 1
bne @catchUp
lda cwCurrentCode
cmp #CODE_WINDOW_SIZE - 1
bcc @end
@catchUp:
CATCH_UP_CODE
jsr refreshCodeWindow
@end:
.endmacro

; Macro for catching up code and invalidating the code window.
.macro CATCH_UP_CODE ;Warning Invalidates The Code Window Call Refresh Afterwards
ADD_WORD_16 ZP_PTR_CODE, cwCurrentCode, ZP_PTR_CODE

lda #TRUE
sta codeWindowInvalid
.endmacro

; refreshCodeWindow subroutine:
; Refreshes the code window by loading new op codes from the source.
; It takes into account if the code window is invalid and needs to be fully refreshed.
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
    LOAD_CODE_WIN_CODE      ; Load code from the code window
    sta @result             ; Store the result in @result
    
    INC_CODE                ; Increment the code pointer
    
    lda @result             ; Load the result into accumulator A
    ldx #$0                 ; Load zero into index register X
    rts                     ; Return from subroutine

_incCodeBy:
    bra @start
    @jumpAmount: .word $0
    @start:
    sta @jumpAmount         ; Store the jump amount
    stx @jumpAmount + 1     ; Store the jump amount (high byte)

    INC_CODE_BY @jumpAmount ; Increment code pointer by jump amount
    rts                     ; Return from subroutine

.endif
