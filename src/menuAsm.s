.include "x16.inc"

.ifndef MENU_INC

MENU_INC = 1

.import _charSetInited
.import _numOfMenus
.import _the_menu
.import _sizeOfMenu
.import _offsetOfText
.import _bFMenuAllowed
.import _bFMenuShown
.import _bFMenuSelected
.import _bFMenuChildSelected

.segment "ZEROPAGE"
MENU_SREG: .word $0
TEXT_ZP: .word $0
MENU_TEXT_COUNTER: .byte $0
OPEN_MENU_ADDRESS: .word $0

MENU_NOT_SELECTED = $10
MENU_SELECTED = $20
MAX_MENU_CHILDREN = 10
MENU_CHILD_TILES = MAX_MENU_CHILDREN * MENU_BAR_WIDTH
MENU_CHILDREN_ADDRESS = MENU_BAR_LOCATION + MENU_BAR_WIDTH
.segment "BANKRAM0F"
bFClearMenuChildren:
stz VERA_ctrl
lda #<MENU_CHILDREN_ADDRESS
sta VERA_addr_low
lda #>MENU_CHILDREN_ADDRESS
sta VERA_addr_high
lda #$20
sta VERA_addr_bank

ldx #<(MENU_CHILD_TILES - 1)
ldy #>(MENU_CHILD_TILES - 1)
lda #TRANSPARENT


@clearLoop:
sta VERA_data0
dex
cpx #$FF
bne @clearLoop
dey
cpy #$FF
bne @clearLoop
@exit:
rts
bFPrintMenuChildText:
jsr bFClearMenuChildren
rts
bFPrintMenuText:
stz MENU_TEXT_COUNTER
ldx #$0

@menusLoop:
cpx _bFMenuSelected
bne @getTextZp

@markOpenMenu:
lda VERA_addr_low
sta OPEN_MENU_ADDRESS
lda VERA_addr_high
sta OPEN_MENU_ADDRESS
@getTextZp:
GET_STRUCT_16_STORED_OFFSET _offsetOfText, MENU_SREG,TEXT_ZP

lda TEXT_ZP
ora TEXT_ZP + 1
beq @end

ldy #$0
@printTextLoop:
lda (TEXT_ZP),y
beq @endPrintTextLoop
sta VERA_data0

cpx _bFMenuSelected
beq @selected
@notSelected:
lda #MENU_NOT_SELECTED
bra @printText
@selected:
lda #MENU_SELECTED
@printText:
sta VERA_data0

inc MENU_TEXT_COUNTER
iny
bra @printTextLoop

@endPrintTextLoop:
lda #SPACE
sta VERA_data0

lda #MENU_NOT_SELECTED
sta VERA_data0

inc MENU_TEXT_COUNTER

clc
lda MENU_SREG
adc _sizeOfMenu
sta MENU_SREG
lda MENU_SREG + 1
adc #$0
sta MENU_SREG + 1
inx
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

lda #$1
sta _bFMenuAllowed

stz _bFMenuShown
stz _bFMenuSelected

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
lda #$10 
sta VERA_addr_bank

ldy #DISPLAY_MENU_FLAG

lda _bFMenuAllowed
beq @clearMenu
GET_FLAG_NON_INTERPRETER MENU_SREG
beq @clearMenu
lda _bFMenuShown
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
ldy #MENU_NOT_SELECTED
@padLoop:
stx VERA_data0
sty VERA_data0
dec
bne @padLoop  


jsr bFPrintMenuChildText

bra @return

@clearMenu:
ldx #MENU_BAR_WIDTH

ldy #TRANSPARENT
@clearMenuLoop:
sty VERA_data0
lda #MENU_NOT_SELECTED
sta VERA_data0

dex
bne @clearMenuLoop

@return:
rts
.endif