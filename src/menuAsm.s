.include "x16.inc"

.ifndef MENU_INC

MENU_INC = 1

.import _charSetInited
.import _numOfMenus
.import _the_menu
.import _sizeOfMenu
.import _offsetOfText

.segment "ZEROPAGE"
MENU_SREG: .word $0
TEXT_ZP: .word $0
MENU_TEXT_COUNTER: .byte $0

.segment "BANKRAM0F"
bFPrintMenuText:
stz MENU_TEXT_COUNTER

@menusLoop:
GET_STRUCT_16_STORED_OFFSET _offsetOfText, MENU_SREG,TEXT_ZP

lda TEXT_ZP
ora TEXT_ZP + 1
beq @end

ldy #$0
@printTextLoop:
lda (TEXT_ZP),y
beq @endPrintTextLoop
sta VERA_data0
inc MENU_TEXT_COUNTER
iny
bra @printTextLoop

@endPrintTextLoop:
lda #SPACE
sta VERA_data0
inc MENU_TEXT_COUNTER

clc
lda MENU_SREG
adc _sizeOfMenu
sta MENU_SREG
lda MENU_SREG + 1
adc #$0
sta MENU_SREG + 1
bra @menusLoop

@end:
rts


SPACE = $20
TRANSPARENT = $80

MENU_BAR_LOCATION = $DA00

DISPLAY_MENU_FLAG = 14
MENU_BAR_SECOND_BYTE = $10

_bFInitMenus:
stz VERA_ctrl
lda #<(MENU_BAR_LOCATION + 1)
sta VERA_addr_low
lda #>(MENU_BAR_LOCATION + 1)
sta VERA_addr_high
lda #$20 
sta VERA_addr_bank

ldx #MENU_BAR_WIDTH

lda #MENU_BAR_SECOND_BYTE
@setupMenuSecondByteLoop:
sta VERA_data0
dex
bne @setupMenuSecondByteLoop


rts

bFDisplayMenu:
lda _charSetInited
beq @return

jsr bFDisplayMenuBar

@return:
rts
bFDisplayMenuBar:
stz VERA_ctrl
lda #<MENU_BAR_LOCATION
sta VERA_addr_low
lda #>MENU_BAR_LOCATION
sta VERA_addr_high
lda #$20 
sta VERA_addr_bank

ldy #DISPLAY_MENU_FLAG


GET_FLAG_NON_INTERPRETER MENU_SREG
beq @clearMenu

@displayMenu:

clc
lda #<_the_menu
adc _offsetOfText
sta MENU_SREG
lda #>_the_menu
adc #$0
sta MENU_SREG + 1
jsr bFPrintMenuText


@pad:
sec
lda #MENU_BAR_WIDTH 
sbc MENU_TEXT_COUNTER
ldx #SPACE
@padLoop:
stx VERA_data0
dec
bne @padLoop  


bra @return

@clearMenu:
ldx #MENU_BAR_WIDTH

lda #TRANSPARENT
@clearMenuLoop:
sta VERA_data0
dex
bne @clearMenuLoop

@return:
rts
.endif