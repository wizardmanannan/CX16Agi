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
.import _bFInitMenuState
.import _bFMenuChildWidth
.import _bFFirstMenuChild
.import _bFMenuChildShiftBack
.import _bFEnabledMenuControllers
.import _offsetOfController

.segment "ZEROPAGE"
MENU_SREG: .word $0
MENU_SREG2: .word $0
TEXT_ZP: .word $0
MENU_TEXT_COUNTER: .byte $0
OPEN_MENU_ADDRESS: .word $0
HAS_ALL_TEXT_BEING_DRAWN: .byte $0
CHILD_MENU_COUNTER = MENU_TEXT_COUNTER ;These are never used at the same time, saving space
CONTROLLER: .byte $0
CONTROLLER_ENABLED = CONTROLLER ;There are never used at the same time

HORIZONAL_BORDER = $5F
VERTICAL_BORDER = $84
MENU_TOP_END = $82
MENU_TOP = $83



CHILD_MENU_PALETTE = $10

MENU_NOT_SELECTED = $10
MENU_SELECTED = $20
MENU_SELECTED_DISABLED = $30
MAX_MENU_CHILDREN = 10
MENU_CHILD_TILES = (MAX_MENU_CHILDREN + 3) * MENU_BAR_WIDTH
MENU_CHILDREN_ADDRESS = MENU_BAR_LOCATION + (TILE_LAYER_WIDTH * 2)

.segment "BANKRAM0F"

bFMoveVeraAddressToNextChildMenu:
ldx _bFMenuSelected
lda _bFMenuChildWidth,x
asl
sta MENU_SREG2

sec
lda VERA_addr_low
sbc MENU_SREG2
sta VERA_addr_low
lda VERA_addr_high
sbc #$0
sta VERA_addr_high

clc
lda VERA_addr_low
adc #TILE_LAYER_WIDTH * 2
sta VERA_addr_low
lda #$0
adc VERA_addr_high
sta VERA_addr_high


rts

bFDrawTopChildrenBorder:
ldy _bFMenuSelected
ldx _bFMenuChildWidth,y
ldy #CHILD_MENU_PALETTE
dex
dex

lda #MENU_TOP_END
sta VERA_data0
sty VERA_data0

lda #MENU_TOP
@drawBorderLoop:
sta VERA_data0
sty VERA_data0

dex
bne @drawBorderLoop

lda #MENU_TOP_END
sta VERA_data0
ldy #CHILD_MENU_PALETTE | 4 ;Flip horizonally
sty VERA_data0

rts

bFDrawBottomChildrenBorder:
ldy _bFMenuSelected
ldx _bFMenuChildWidth,y
ldy #CHILD_MENU_PALETTE | 8 ;Flip vertically
dex
dex

lda #MENU_TOP_END
sta VERA_data0
sty VERA_data0

lda #MENU_TOP
@drawBorderLoop:
sta VERA_data0
sty VERA_data0

dex
bne @drawBorderLoop

lda #MENU_TOP_END
sta VERA_data0
ldy #CHILD_MENU_PALETTE | 4 | 8 ;Flip vertically and horizonally
sty VERA_data0

rts

bFDisplayChildMenu:
stz VERA_ctrl

clc
lda OPEN_MENU_ADDRESS
adc #TILE_LAYER_WIDTH * 2
sta VERA_addr_low
lda OPEN_MENU_ADDRESS + 1
adc #$0
sta VERA_addr_high
lda #$10
sta VERA_addr_bank

lda #MENU_TOP
jsr bFDrawTopChildrenBorder
jsr bFMoveVeraAddressToNextChildMenu

lda _bFMenuSelected
asl
tax
lda _bFFirstMenuChild,x
sta MENU_SREG
inx
lda _bFFirstMenuChild,x
sta MENU_SREG + 1

stz CHILD_MENU_COUNTER

@childMenuLoop:
GET_STRUCT_16_STORED_OFFSET _offsetOfText, MENU_SREG,TEXT_ZP
GET_STRUCT_8_STORED_OFFSET _offsetOfController, MENU_SREG,CONTROLLER

lda _offsetOfController
ldy CONTROLLER
lda MENU_SREG
lda _bFEnabledMenuControllers,y
sta CONTROLLER_ENABLED


lda TEXT_ZP
ora TEXT_ZP + 1
bne @loadWidth
jmp @drawBottomBorder


@loadWidth:
ldx _bFMenuSelected
ldy _bFMenuChildWidth,x
sty MENU_SREG2

ldy #$0
stz HAS_ALL_TEXT_BEING_DRAWN
@drawMenuItemLoop:

@checkDrawRightBorder:
lda MENU_SREG2
dec
sta MENU_SREG2 + 1
cpy MENU_SREG2 + 1
bne @checkDrawLeftBorder
@drawRightBorder: 
lda #VERTICAL_BORDER
sta VERA_data0
lda #CHILD_MENU_PALETTE | 4 ;Flip horizonally to make right border
sta VERA_data0
bra @moveToNextLine

@checkDrawLeftBorder:
cpy #$0
bne @checkDrawText

@drawLeftBorder:
lda #VERTICAL_BORDER
sta VERA_data0

lda #CHILD_MENU_PALETTE
sta VERA_data0
iny
bra @drawMenuItemLoop

@checkDrawText:
lda HAS_ALL_TEXT_BEING_DRAWN
bne @drawPadding

dey ;Text will be one behind x due to left border
lda (TEXT_ZP),y
iny

cmp #$0
bne @drawText

@terminatorFound:
inc HAS_ALL_TEXT_BEING_DRAWN
bra @drawPadding

@drawText:
sta VERA_data0

lda CHILD_MENU_COUNTER
cmp _bFMenuChildSelected
beq @isSelected

@isNotSelected:
lda #CHILD_MENU_PALETTE
sta VERA_data0
bra @incrementToNextLetter

@isSelected:
lda CONTROLLER_ENABLED
beq @isNotEnabled

@isEnabled:    
lda #MENU_SELECTED
sta VERA_data0
bra @incrementToNextLetter

@isNotEnabled:
lda #MENU_SELECTED_DISABLED
sta VERA_data0

@incrementToNextLetter:
iny 
bra @drawMenuItemLoop

@drawPadding:
lda #SPACE
sta VERA_data0
lda #CHILD_MENU_PALETTE
sta VERA_data0
iny
bra @drawMenuItemLoop


@moveToNextLine:
jsr bFMoveVeraAddressToNextChildMenu

inc CHILD_MENU_COUNTER

clc
lda _sizeOfMenu
adc MENU_SREG
sta MENU_SREG
lda #$0
adc MENU_SREG + 1
sta MENU_SREG + 1

jmp @childMenuLoop

@drawBottomBorder:
lda #HORIZONAL_BORDER
jsr bFDrawBottomChildrenBorder

rts


bFClearMenuChildren:
lda #%00001100
sta VERA_ctrl

lda #TRANSPARENT
ldy #CHILD_MENU_PALETTE
sta $9f29
sty $9f2A
sta $9f2B
sty $9f2C

lda #<MENU_CHILDREN_ADDRESS
sta VERA_addr_low
lda #>MENU_CHILDREN_ADDRESS
sta VERA_addr_high
lda #$30
sta VERA_addr_bank

lda MENU_CHILD_TILES
ldx #<((MENU_CHILD_TILES - 1) / 2) ;It writes four at a time, but only two of those are tiles, the other two are the palettes
ldy #>((MENU_CHILD_TILES - 1) / 2)

 ; Set up VERA for cache operations
lda #%00000100  ; DCSEL = Mode 2 for enabling cache
sta VERA_ctrl
lda #%01000000  ; Enable cache writing
sta VERA_dc_video

@clearLoop:
sta VERA_data0
dex
cpx #$FF
bne @clearLoop
dey
cpy #$FF
bne @clearLoop
@exit:

stz VERA_dc_video
stz VERA_ctrl

rts

bFPrintMenuChildText:
jsr bFClearMenuChildren
jsr bFDisplayChildMenu
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
sta OPEN_MENU_ADDRESS + 1

lda _bFMenuChildShiftBack,x
beq @getTextZp

sec
lda OPEN_MENU_ADDRESS
sbc _bFMenuChildShiftBack,x
sta OPEN_MENU_ADDRESS
lda OPEN_MENU_ADDRESS + 1
sbc #$0
sta OPEN_MENU_ADDRESS + 1

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
MENU_BAR_END = MENU_BAR_LOCATION + (MENU_BAR_WIDTH * 2)
MENU_BAR_MAX_CHILD_FIRST_ROW = (MENU_BAR_END + TILE_LAYER_WIDTH * 2)

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

jsr _bFInitMenuState
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

stz _menuDirty

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

jsr bFClearMenuChildren

@return:
rts
.endif