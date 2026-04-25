.include "x16.inc"

.ifndef MENU_INC

MENU_INC = 1

.import _charSetInited

.segment "ZEROPAGE"
menuSreg: .word $0

.segment "BANKRAM0F"


SPACE = $20

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


GET_FLAG_NON_INTERPRETER menuSreg
beq @clearMenu

@displayMenu:

bra @return

@clearMenu:
ldx #MENU_BAR_WIDTH

lda #SPACE
@clearMenuLoop:
sta VERA_data0
dex
bne @clearMenuLoop

@return:
rts
.endif