.include "x16.inc"                  ; Include Commander X16 hardware definitions

.ifndef MENU_INC                    ; Prevent multiple inclusion of this header

MENU_INC = 1                        ; Define include guard

.import _charSetInited              ; Import external variables
.import _numOfMenus
.import _the_menu
.import _sizeOfMenu
.import _offsetOfText
.import _menuAllowed
.import _menuShown
.import _bAMenuSelected
.import _bAMenuChildSelected
.import _bAInitMenuState
.import _bAMenuChildWidth
.import _bAFirstMenuChild
.import _bAMenuChildShiftBack
.import _bAEnabledMenuControllers
.import _offsetOfController

.segment "ZEROPAGE"                 ; Variables in zero page for speed
MENU_SREG = IRQ_TMP_1               ; Main 16-bit working register
MENU_SREG2 = IRQ_TMP_2              ; Secondary working register
TEXT_ZP = IRQ_TMP_3                 ; Pointer to text string
OPEN_MENU_ADDRESS = IRQ_TMP_4       ; VERA address of open menu
MENU_TEXT_COUNTER = IRQ_TMP_5       ; Counter for printed characters
HAS_ALL_TEXT_BEING_DRAWN = IRQ_TMP_5 + 1  ; Flag for text completion
CHILD_MENU_COUNTER = MENU_TEXT_COUNTER ; Reused ZP byte (saves space)
CONTROLLER = IRQ_TMP_6              ; Controller index
CONTROLLER_ENABLED = CONTROLLER     ; Reused for enabled state flag

HORIZONAL_BORDER = $5F              ; Tile for horizontal border
VERTICAL_BORDER = $84               ; Tile for vertical border
MENU_TOP_END = $82                  ; Tile for top border corners
MENU_TOP = $83                      ; Tile for top border line

CHILD_MENU_PALETTE = $10            ; Base palette for child menus

MENU_NOT_SELECTED = $10             ; Attribute byte for unselected items
MENU_SELECTED = $20                 ; Attribute byte for selected items
MENU_SELECTED_DISABLED = $30        ; Attribute byte for disabled selected items

MAX_MENU_CHILDREN = 10              ; Maximum number of child menu items
MENU_CHILD_TILES = (MAX_MENU_CHILDREN + 3) * MENU_BAR_WIDTH  ; Total tiles needed
MENU_CHILDREN_ADDRESS = MENU_BAR_LOCATION + (TILE_LAYER_WIDTH * 2)  ; VRAM location for children


.segment "CODE"                     ; Start of executable code

; ================================================================
; isMenuAllowed
; ================================================================
; Purpose: Checks whether the menu system is allowed to be displayed
;          based on the global enable flags.
; Input:   None (reads _menuAllowed and system flags)
; Output:  A = 0 if menu is not allowed, non-zero if allowed
; ================================================================
isMenuAllowed:                      ; Function: Check if menu system is enabled
lda _menuAllowed                    ; Load menu allowed flags
tax                                 ; Save copy in X
ldy #ENABLE_MENU                    ; Load flag bit position
GET_FLAG_NON_INTERPRETER MENU_SREG  ; Get system flag (macro)
sta MENU_SREG                       ; Store result
txa                                 ; Restore original flags
and MENU_SREG                       ; Mask with allowed bits
rts                                 ; Return result in A (0 = not allowed)


.segment "BANKRAM0A"                ; Code in banked RAM area

; ================================================================
; bAClearTopLine
; ================================================================
; Purpose: Clears the top menu bar line in VRAM with transparent 
;          tiles and unselected palette attributes.
; Input:   None
; Output:  None (modifies VERA VRAM directly)
; ================================================================
bAClearTopLine:                     ; Clear the top menu bar line

stz VERA_ctrl                       ; Reset VERA control register
lda #<MENU_BAR_LOCATION             ; Set low byte of VRAM address
sta VERA_addr_low
lda #>MENU_BAR_LOCATION             ; Set high byte of VRAM address
sta VERA_addr_high
lda #$10                            ; Set bank + increment mode
sta VERA_addr_bank

ldx #MENU_BAR_WIDTH                 ; Width of menu bar in tiles

ldy #TRANSPARENT                    ; Transparent tile value
@clearMenuLoop:                     ; Loop to clear each tile
sty VERA_data0                      ; Write transparent tile
lda #MENU_NOT_SELECTED              ; Load unselected attribute
sta VERA_data0                      ; Write attribute byte
dex                                 ; Decrement counter
bne @clearMenuLoop                  ; Repeat until done
rts                                 ; Return


; ================================================================
; bAMoveVeraAddressToNextChildMenu
; ================================================================
; Purpose: Adjusts the current VERA address to point to the next 
;          row in the child menu area, accounting for menu width.
; Input:   VERA address set, _bAMenuSelected, _bAMenuChildWidth
; Output:  Updated VERA_addr_low/high
; ================================================================
bAMoveVeraAddressToNextChildMenu:   ; Move VERA pointer to next child menu row
ldx _bAMenuSelected                 ; Get currently selected menu
lda _bAMenuChildWidth,x             ; Load width of its child menu
asl                                 ; Multiply by 2 (two bytes per tile)
sta MENU_SREG2                      ; Store offset

sec                                 ; Prepare subtraction
lda VERA_addr_low                   ; Subtract child width offset
sbc MENU_SREG2
sta VERA_addr_low
lda VERA_addr_high
sbc #$0
sta VERA_addr_high

clc                                 ; Prepare addition for next row
lda VERA_addr_low
adc #TILE_LAYER_WIDTH * 2           ; Move down one full row (2 bytes/tile)
sta VERA_addr_low
lda #$0
adc VERA_addr_high
sta VERA_addr_high

rts


; ================================================================
; bADrawTopChildrenBorder
; ================================================================
; Purpose: Draws the top border (including corners) of a dropdown 
;          child menu using special border tiles and palette.
; Input:   _bAMenuSelected, _bAMenuChildWidth, VERA address set
; Output:  None (writes directly to VERA)
; ================================================================
bADrawTopChildrenBorder:            ; Draw top border of dropdown menu
ldy _bAMenuSelected                 ; Get selected menu index
ldx _bAMenuChildWidth,y             ; Load child menu width
ldy #CHILD_MENU_PALETTE             ; Load child palette
dex                                 ; Adjust for borders
dex

lda #MENU_TOP_END                   ; Left corner tile
sta VERA_data0
sty VERA_data0                      ; Palette byte

lda #MENU_TOP                       ; Top border tile
@drawBorderLoop:
sta VERA_data0                      ; Write tile
sty VERA_data0                      ; Write palette
dex
bne @drawBorderLoop                 ; Loop across width

lda #MENU_TOP_END                   ; Right corner tile
sta VERA_data0
ldy #CHILD_MENU_PALETTE | 4         ; Flip horizontally
sty VERA_data0

rts


; ================================================================
; bADrawBottomChildrenBorder
; ================================================================
; Purpose: Draws the bottom border (including corners) of a dropdown 
;          child menu with appropriate vertical/horizontal flips.
; Input:   _bAMenuSelected, _bAMenuChildWidth, VERA address set
; Output:  None (writes directly to VERA)
; ================================================================
bADrawBottomChildrenBorder:         ; Draw bottom border of dropdown menu
ldy _bAMenuSelected
ldx _bAMenuChildWidth,y
ldy #CHILD_MENU_PALETTE | 8         ; Flip vertically
dex
dex

lda #MENU_TOP_END                   ; Left bottom corner
sta VERA_data0
sty VERA_data0

lda #MENU_TOP                       ; Bottom border line
@drawBorderLoop:
sta VERA_data0
sty VERA_data0
dex
bne @drawBorderLoop

lda #MENU_TOP_END                   ; Right bottom corner
sta VERA_data0
ldy #CHILD_MENU_PALETTE | 4 | 8     ; Flip both ways
sty VERA_data0

rts


; ================================================================
; bADisplayChildMenu
; ================================================================
; Purpose: Main routine that draws an entire child (dropdown) menu.
;          Handles borders, text items, selection highlighting, 
;          controller enable states, and padding.
; Input:   OPEN_MENU_ADDRESS, _bAMenuSelected, menu data structures
; Output:  None (renders directly to VERA)
; ================================================================
bADisplayChildMenu:                 ; Draw complete child (dropdown) menu
stz VERA_ctrl                       ; Reset VERA control

clc
lda OPEN_MENU_ADDRESS               ; Calculate position below menu bar
adc #TILE_LAYER_WIDTH * 2
sta VERA_addr_low
lda OPEN_MENU_ADDRESS + 1
adc #$0
sta VERA_addr_high
lda #$10
sta VERA_addr_bank

lda #MENU_TOP
jsr bADrawTopChildrenBorder         ; Draw top border
jsr bAMoveVeraAddressToNextChildMenu ; Move to first item row

lda _bAMenuSelected                 ; Get pointer to first child item
asl
tax
lda _bAFirstMenuChild,x
sta MENU_SREG
inx
lda _bAFirstMenuChild,x
sta MENU_SREG + 1

stz CHILD_MENU_COUNTER              ; Reset child counter

@childMenuLoop:                     ; Loop through each child menu item
GET_STRUCT_16_STORED_OFFSET _offsetOfText, MENU_SREG,TEXT_ZP
GET_STRUCT_8_STORED_OFFSET _offsetOfController, MENU_SREG,CONTROLLER

lda _offsetOfController             ; Check controller enabled state
ldy CONTROLLER
lda _bAEnabledMenuControllers,y
sta CONTROLLER_ENABLED

lda TEXT_ZP                         ; Check if valid text pointer
ora TEXT_ZP + 1
bne @loadWidth
jmp @drawBottomBorder               ; No more items - draw bottom


@loadWidth:
ldx _bAMenuSelected
ldy _bAMenuChildWidth,x
sty MENU_SREG2                      ; Store width for border checks

ldy #$0
stz HAS_ALL_TEXT_BEING_DRAWN        ; Reset text done flag
@drawMenuItemLoop:                  ; Draw one line of menu item

; ---------------------------------------------------------------
; Border and text drawing logic for each character position
; ---------------------------------------------------------------

@checkDrawRightBorder:              ; Check if we are at the rightmost position of the menu item
                                    ; (last column before right border)
lda MENU_SREG2                      ; Load current remaining width
dec                                 ; Decrement to get actual last index
sta MENU_SREG2 + 1                  ; Store for comparison
cpy MENU_SREG2 + 1                  ; Compare current Y (column) with last position
bne @checkDrawLeftBorder            ; Not at right edge → continue checks
@drawRightBorder: 
lda #VERTICAL_BORDER                ; Draw right vertical border tile
sta VERA_data0
lda #CHILD_MENU_PALETTE | 4         ; Right border (flipped horizontally)
sta VERA_data0
bra @moveToNextLine                 ; Done with this row, move to next line

@checkDrawLeftBorder:               ; Check if we are at the leftmost position (first column)
cpy #$0                             ; Y == 0 means start of line
bne @checkDrawText                  ; Not left edge → draw text/padding

@drawLeftBorder:                    ; Draw left vertical border
lda #VERTICAL_BORDER                ; Left border tile
sta VERA_data0
lda #CHILD_MENU_PALETTE             ; Normal palette for left border
sta VERA_data0
iny                                 ; Advance column counter past border
bra @drawMenuItemLoop               ; Restart loop for next column

@checkDrawText:                     ; Decide whether to draw text, padding, or terminator handling
lda HAS_ALL_TEXT_BEING_DRAWN        ; Have we already hit the null terminator?
bne @drawPadding                    ; Yes → fill rest of line with padding

dey                                 ; Adjust Y back one (because left border took a slot)
lda (TEXT_ZP),y                     ; Read next character from string
iny                                 ; Restore Y

cmp #$0                             ; Is it the null terminator?
bne @drawText

@terminatorFound:                   ; End of string reached
inc HAS_ALL_TEXT_BEING_DRAWN        ; Set flag so remaining columns become padding
bra @drawPadding

@drawText:                          ; Draw actual character from menu item text
sta VERA_data0

lda CHILD_MENU_COUNTER              ; Check if this row is the currently selected item
cmp _bAMenuChildSelected
beq @isSelected

@isNotSelected:                     ; Normal unselected row
lda #CHILD_MENU_PALETTE
sta VERA_data0
bra @incrementToNextLetter

@isSelected:                        ; This row is selected - check enable state
lda CONTROLLER_ENABLED
beq @isNotEnabled

@isEnabled:                         ; Enabled selected item (bright color)
lda #MENU_SELECTED
sta VERA_data0
bra @incrementToNextLetter

@isNotEnabled:                      ; Selected but disabled (dimmed appearance)
lda #MENU_SELECTED_DISABLED
sta VERA_data0

@incrementToNextLetter:
iny                                 ; Move to next character/column
bra @drawMenuItemLoop               ; Continue drawing this row

@drawPadding:                       ; Fill remaining space with blank tiles after text ends
lda #SPACE                          ; Space character tile
sta VERA_data0
lda #CHILD_MENU_PALETTE             ; Normal palette
sta VERA_data0
iny                                 ; Advance column
bra @drawMenuItemLoop               ; Continue until right border is hit


@moveToNextLine:
jsr bAMoveVeraAddressToNextChildMenu

inc CHILD_MENU_COUNTER              ; Move to next menu child item

clc
lda _sizeOfMenu                     ; Advance to next menu struct
adc MENU_SREG
sta MENU_SREG
lda #$0
adc MENU_SREG + 1
sta MENU_SREG + 1

jmp @childMenuLoop

@drawBottomBorder:
lda #HORIZONAL_BORDER
jsr bADrawBottomChildrenBorder

rts


; ================================================================
; bAClearMenuChildren
; ================================================================
; Purpose: Fast clear of the entire child menu area in VRAM using 
;          VERA's cache write mode for performance.
; Input:   None
; Output:  None (clears VRAM region)
; Notes:   This routine writes in groups of 4 bytes via VERA cache.
;          It may overflow by up to 3 extra tiles. We don't care
;          because the overflow writes transparent tiles off-screen
;          or in unused VRAM, which has no visible effect.
; ================================================================
bAClearMenuChildren:                ; Fast clear of entire child menu area

; Step 1: Configure VERA control register for cache operations
lda #%00001100                      ; DCSEL=3 (Mode 3) + other bits to prepare for fill
sta VERA_ctrl

; Step 2: Set up the 4-byte cache fill pattern (Tile + Attribute repeated)
; $9f29-$9f2C are the VERA cache data registers when in the right DCSEL mode
lda #TRANSPARENT                    ; Tile byte = transparent (clear visual)
ldy #CHILD_MENU_PALETTE             ; Attribute byte = child menu palette
sta $9f29                           ; Cache byte 0: Tile
sty $9f2A                           ; Cache byte 1: Attribute
sta $9f2B                           ; Cache byte 2: Tile (repeat)
sty $9f2C                           ; Cache byte 3: Attribute (repeat)

; Step 3: Set destination VRAM address for the child menu area
lda #<MENU_CHILDREN_ADDRESS
sta VERA_addr_low
lda #>MENU_CHILDREN_ADDRESS
sta VERA_addr_high
lda #$30                            ; Bank 0 + auto-increment by 4 (important for cache writes)
sta VERA_addr_bank

; Step 4: Prepare loop counters
; Note: VERA cache writes 4 bytes at a time, but only 2 are actual tile data (tile+attr)
lda MENU_CHILD_TILES                ; Total number of tile+attr pairs to clear
ldx #<((MENU_CHILD_TILES - 1) / 2)  ; Low byte of (tiles / 2) - 1
ldy #>((MENU_CHILD_TILES - 1) / 2)  ; High byte of (tiles / 2) - 1

; Step 5: Enable VERA data cache write mode (very fast block fill)
lda #%00000100                      ; DCSEL=2 (Mode 2) - required for cache control
sta VERA_ctrl
lda #%01000000                      ; Enable cache write (writes all 4 cache bytes on each VERA_data0 access)
sta VERA_dc_video

; Step 6: Fast clear loop using 16-bit counter (X = low, Y = high)
@clearLoop:
sta VERA_data0                      ; Write 4 bytes (tile+attr repeated) to VRAM via cache
dex                                 ; Decrement 16-bit counter
cpx #$FF                            ; Detect underflow from $00 -> $FF
bne @clearLoop
dey
cpy #$FF                            ; High byte underflow?
bne @clearLoop
@exit:

; Step 7: Clean up VERA state
stz VERA_dc_video                   ; Disable cache write mode
stz VERA_ctrl                       ; Reset control register

rts


; ================================================================
; bAPrintMenuChildText
; ================================================================
; Purpose: High-level routine to redraw the child menu.
;          Clears the area then calls the full display routine.
; Input:   None
; Output:  None
; ================================================================
bAPrintMenuChildText:               ; Public: redraw child menu
jsr bAClearMenuChildren
jsr bADisplayChildMenu
rts


; ================================================================
; bAPrintMenuText
; ================================================================
; Purpose: Prints all top-level menu bar items, handles selection 
;          highlighting, padding, and saves open menu address.
; Input:   MENU_SREG pointing to menu data, _bAMenuSelected
; Output:  None (writes to VERA)
; ================================================================
bAPrintMenuText:                    ; Print top level menu bar items
stz MENU_TEXT_COUNTER               ; Reset character counter for padding calculation later
ldx #$0                             ; X = menu item index (0 = first top-level menu)

@menusLoop:                         ; Main loop: process each top-level menu item
cpx _bAMenuSelected                 ; Is this the currently selected menu?
bne @getTextZp

@markOpenMenu:                      ; This is the open/selected menu - save its VRAM position
lda VERA_addr_low                   ; Save current low address byte
sta OPEN_MENU_ADDRESS
lda VERA_addr_high                  ; Save current high address byte
sta OPEN_MENU_ADDRESS + 1

lda _bAMenuChildShiftBack,x         ; Check if we need to shift left for child alignment
beq @getTextZp

sec
lda OPEN_MENU_ADDRESS               ; Apply horizontal shift for better child menu positioning
sbc _bAMenuChildShiftBack,x
sta OPEN_MENU_ADDRESS
lda OPEN_MENU_ADDRESS + 1
sbc #$0
sta OPEN_MENU_ADDRESS + 1

@getTextZp:                         ; Load pointer to current menu item's text
GET_STRUCT_16_STORED_OFFSET _offsetOfText, MENU_SREG,TEXT_ZP

lda TEXT_ZP                         ; Check if text pointer is valid (not NULL)
ora TEXT_ZP + 1
beq @end                            ; No more items → finish

ldy #$0                             ; Y = character index in string
@printTextLoop:                     ; Inner loop: print one character + attribute
lda (TEXT_ZP),y                     ; Read next character
beq @endPrintTextLoop               ; Null terminator → end of this item
sta VERA_data0                      ; Write character tile to VERA

cpx _bAMenuSelected                 ; Highlight if this is the selected top-level item
beq @selected
@notSelected:
lda #MENU_NOT_SELECTED              ; Dim palette for unselected
bra @printText
@selected:
lda #MENU_SELECTED                  ; Bright palette for selected
@printText:
sta VERA_data0                      ; Write attribute byte

inc MENU_TEXT_COUNTER               ; Track total characters printed
iny                                 ; Next character
bra @printTextLoop

@endPrintTextLoop:                  ; Finished printing text for this item
lda #SPACE                          ; Add trailing space for separation
sta VERA_data0

lda #MENU_NOT_SELECTED              ; Use unselected palette for padding space
sta VERA_data0

inc MENU_TEXT_COUNTER               ; Count the padding space

clc
lda MENU_SREG                       ; Advance MENU_SREG to next menu struct
adc _sizeOfMenu
sta MENU_SREG
lda MENU_SREG + 1
adc #$0
sta MENU_SREG + 1
inx                                 ; Next top-level menu item
bra @menusLoop

@end:
rts


MENU_BAR_LOCATION = $DA00           ; VRAM address for menu bar
MENU_BAR_END = MENU_BAR_LOCATION + (MENU_BAR_WIDTH * 2)
MENU_BAR_MAX_CHILD_FIRST_ROW = (MENU_BAR_END + TILE_LAYER_WIDTH * 2)

MENU_BAR_SECOND_BYTE = $10


; ================================================================
; _bAInitMenus
; ================================================================
; Purpose: Initializes the menu bar hardware setup (mainly attribute 
;          bytes) and calls external state initialization.
; Input:   None
; Output:  None
; ================================================================
_bAInitMenus:                       ; Initialize menu hardware setup
stz VERA_ctrl
lda #<(MENU_BAR_LOCATION + 1)       ; Point to attribute bytes (second byte of each tile)
sta VERA_addr_low
lda #>(MENU_BAR_LOCATION + 1)
sta VERA_addr_high
lda #$20                            ; Bank + increment by 1
sta VERA_addr_bank

ldx #MENU_BAR_WIDTH

lda #MENU_BAR_SECOND_BYTE           ; Default attribute value for menu bar
@setupMenuSecondByteLoop:
sta VERA_data0
dex
bne @setupMenuSecondByteLoop

jsr _bAInitMenuState                ; Call external state initialization routine
rts


; ================================================================
; bADisplayMenu
; ================================================================
; Purpose: Main public entry point for displaying the menu system.
;          Checks if charset is initialized before proceeding.
; Input:   None
; Output:  None
; ================================================================
bADisplayMenu:                      ; Main public menu display function
lda _charSetInited                  ; Check if charset is ready
beq @return

jsr bADisplayMenuBar

@return:
rts


; ================================================================
; bADisplayMenuBar
; ================================================================
; Purpose: Core routine that either displays the full menu bar + 
;          child menu or clears everything, based on flags.
; Input:   Menu state variables
; Output:  None (updates VRAM)
; ================================================================
bADisplayMenuBar:                   ; Display or hide the full menu bar
stz VERA_ctrl
lda #<MENU_BAR_LOCATION             ; Set VRAM address to start of menu bar
sta VERA_addr_low
lda #>MENU_BAR_LOCATION
sta VERA_addr_high
lda #$10                            ; Bank 0 + increment by 1
sta VERA_addr_bank

stz _menuDirty                      ; Clear dirty flag (used by caller for optimization)

jsr isMenuAllowed
beq @clearMenu                      ; Menu not allowed → clear it

lda _menuShown                      ; Is the menu currently visible?
beq @clearMenu

@displayMenu:                       ; Menu is allowed and shown → draw it
clc
lda #<_the_menu                     ; Point MENU_SREG to start of menu data structure
adc _offsetOfText
sta MENU_SREG
lda #>_the_menu
adc #$0
sta MENU_SREG + 1
jsr bAPrintMenuText                 ; Print all top-level menu items


@pad:                               ; Fill remaining space in menu bar with blanks
sec
lda #MENU_BAR_WIDTH                 ; Calculate how many padding tiles needed
sbc MENU_TEXT_COUNTER
ldx #SPACE
ldy #MENU_NOT_SELECTED
@padLoop:
stx VERA_data0                      ; Write space tile
sty VERA_data0                      ; Write unselected attribute
dec
bne @padLoop  

jsr bAPrintMenuChildText            ; Draw the dropdown child menu if applicable

bra @return

@clearMenu:                         ; Hide menu - clear both bar and children
jsr bAClearTopLine
jsr bAClearMenuChildren

@return:
rts
.endif