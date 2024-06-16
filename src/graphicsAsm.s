.ifndef GRAPHICS_INC
; Set the value of include guard and define constants
GRAPHICS_INC = 1

.include "global.s"
.include "globalGraphics.s"
.include "helpersAsm.s"
.include "irqAsm.s"

.segment "BANKRAM03"
_lastBoxLines: .byte $0 ; Wish I could have declared these in textLayer.c, but put it here to ensure it is in bank 3
_lastBoxStartLine: .byte $0


_textBuffer1: .res 1000
_textBuffer2: .res 1000


 .segment "BANKRAM06"



.import _b3InitCharset
.import pushax
.import pusha
.import popa
.import _b6DisplayLoadingScreen
.import _b6InitVeraMemory
.import _b9InitSpriteData
.import _b3SetTextColor


_b6Clear:
TRAMPOLINE #TEXT_BANK, _b3InitLayer1Mapbase
TRAMPOLINE #SPRITE_UPDATES_BANK, _bEClearSpriteAttributes
TRAMPOLINE #PICTURE_CODE_OVERFLOW_BANK, _b4ClearPicture
TRAMPOLINE #GRAPHICS_BANK, _b6InitInput
rts

@mapWidth: .byte $0
@isFirstPixel: .byte $0
@loopCounter: .byte $0
_b6InitBackground:
lda #$10
sta VERA_addr_bank
stz VERA_addr_high
stz VERA_addr_low

; Calculate number of bytes per row (BITMAP_WIDTH / 2) and store it into X
lda #<(BITMAP_WIDTH / 2)
tax

; Calculate number of rows (BITMAP_HEIGHT) and store it into @mapHeight
lda #<BITMAP_HEIGHT
sta @mapHeight

@loopOuter:
    ldy @mapHeight  ; Load Y with mapHeight
    @loopInner:
        stz VERA_data0  ; Store 0 into VRAM (set pixel to black)
        dey  ; Decrement Y
        bne @loopInner  ; If Y is not 0, continue loop
    dex  ; Decrement X
    bne @loopOuter  ; If X is not 0, continue loop
rts
@mapHeight: .byte $0

_b6InitGraphics:
SEND_IRQ_COMMAND #IRQ_CMD_BLACKSCREEN, @vSyncToCheck

WAIT_FOR_NEXT_IRQ @vSyncToCheck

jsr _b6InitVeraMemory
TRAMPOLINE #SPRITE_MANAGER_BANK, _b9InitSpriteData

sei
lda #DISPLAY_SCALE
sta VERA_dc_hscale
sta VERA_dc_vscale

stz VERA_ctrl
lda #^VRAM_palette | $10
sta VERA_addr_bank
lda #>VRAM_palette
sta VERA_addr_high
lda #<VRAM_palette
sta VERA_addr_low

;Bitmap Layer 0
lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLUE
sta VERA_data0
lda #>COLOR_BLUE
sta VERA_data0

lda #<COLOR_GREEN
sta VERA_data0
lda #>COLOR_GREEN
sta VERA_data0

lda #<COLOR_CYAN
sta VERA_data0
lda #>COLOR_CYAN
sta VERA_data0

lda #<COLOR_RED
sta VERA_data0
lda #>COLOR_RED
sta VERA_data0

lda #<COLOR_MAGENTA
sta VERA_data0
lda #>COLOR_MAGENTA
sta VERA_data0

lda #<COLOR_BROWN
sta VERA_data0
lda #>COLOR_BROWN
sta VERA_data0

lda #<COLOR_LIGHT_GRAY
sta VERA_data0
lda #>COLOR_LIGHT_GRAY
sta VERA_data0

lda #<COLOR_DARK_GRAY
sta VERA_data0
lda #>COLOR_DARK_GRAY
sta VERA_data0

lda #<COLOR_LIGHT_BLUE
sta VERA_data0
lda #>COLOR_LIGHT_BLUE
sta VERA_data0

lda #<COLOR_LIGHT_GREEN
sta VERA_data0
lda #>COLOR_LIGHT_GREEN
sta VERA_data0

lda #<COLOR_LIGHT_CYAN
sta VERA_data0
lda #>COLOR_LIGHT_CYAN
sta VERA_data0

lda #<COLOR_LIGHT_RED
sta VERA_data0
lda #>COLOR_LIGHT_RED
sta VERA_data0

lda #<COLOR_LIGHT_MAGENTA
sta VERA_data0
lda #>COLOR_LIGHT_MAGENTA
sta VERA_data0

lda #<COLOR_YELLOW
sta VERA_data0
lda #>COLOR_YELLOW
sta VERA_data0

lda #<COLOR_WHITE
sta VERA_data0
lda #>COLOR_WHITE
sta VERA_data0

;TileSet Layer 1
lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

 lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_WHITE
sta VERA_data0
lda #>COLOR_WHITE
sta VERA_data0

lda #<COLOR_RED
sta VERA_data0
lda #>COLOR_RED
sta VERA_data0

;The rest of this palette is junk
lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

;TileSet Layer 2
lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_WHITE
sta VERA_data0
lda #>COLOR_WHITE
sta VERA_data0


lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0


lda #<COLOR_RED
sta VERA_data0
lda #>COLOR_RED
sta VERA_data0


;Other palettes are dynamic created in paletteManager.c

lda #$6   ; Bitmap mode 16 colors
sta VERA_L0_config
stz VERA_L0_tilebase ;A 320 * 240 pixel bitmap at the beginning of VRAM

jsr _b6InitBackground

lda #$11 ; 32 x 64 2bpp tiles
sta VERA_L1_config
lda #$68 ; 0xD000 Address 8x8 width
sta VERA_L1_tilebase
lda #$6D ; 0xDA00 
sta VERA_L1_mapbase

lda #$FC
sta VERA_L1_vscroll_l
stz VERA_L1_vscroll_h

TRAMPOLINE #TEXT_BANK, _b3InitCharset
TRAMPOLINE #TEXT_BANK, _b3InitLayer1Mapbase
TRAMPOLINE #SPRITE_UPDATES_BANK, _bEClearSpriteAttributes 
jsr _b6InitInput

lda #DEFAULT_TEXT_FOREGROUND
ldx #$0
jsr pushax

lda #DEFAULT_TEXT_BACKGROUND
ldx #$0

cli

TRAMPOLINE #TEXT_BANK, _b3SetTextColor 



lda #IRQ_CMD_NORMAL
ldx #$0
jsr _b6SetAndWaitForIrqStateAsm

rts
@vSyncToCheck: .word $0

_b6InitInput:
SET_VERA_ADDRESS_IMMEDIATE_SPLIT #<INPUT_STRING_ADDRESS + 1, #>INPUT_STRING_ADDRESS, #$0, #$0, #$2 ;To to the first tile byte 1
ldx #INPUT_BYTE_1
lda #MAX_INPUT_STRING_LENGTH

@initLoop:
stx VERA_data0

@checkCondition:
dec
bne @initLoop 
rts

.endif