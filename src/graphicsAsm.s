.segment "BANKRAM06"
.ifndef GRAPHICS_INC

.include "global.s"

.import _b6InitCharset
.import _malloc
.import _free
.import pushax
.import pusha
.import popa

; Set the value of include guard and define constants
GRAPHICS_INC = 1

BITMAP_WIDTH = 320
BITMAP_HEIGHT = 240
BYTES_PER_ROW = BITMAP_WIDTH / 2

PICTURE_WIDTH =  160  
PICTURE_HEIGHT =  168

STARTING_ROW = (BITMAP_HEIGHT / 2) - (PICTURE_HEIGHT / 2)
STARTING_BYTE = STARTING_ROW * BYTES_PER_ROW

DEFAULT_BACKGROUND_COLOR = $FF
LEFT_BORDER = $F0
RIGHT_BORDER = $0F

BYTES_PER_CHARACTER = 16
TRANSPARENT_CHAR = $AF
NO_CHARS = 160
SIZE_OF_CHARSET = (BYTES_PER_CHARACTER * NO_CHARS)

_b6ClearBackground:
lda #$10 | ^STARTING_BYTE
sta VERA_addr_bank
lda #> (STARTING_ROW * (BITMAP_WIDTH / 2) ) ;There are 320 bytes per row, but since each pixel is 4 bits we divide by 2
sta VERA_addr_high
lda #< (STARTING_ROW * (BITMAP_WIDTH / 2) )
sta VERA_addr_low

; Calculate number of bytes per row. There are 160 pixel per row, double width. However each pixel is 4 bits, so 160 * 2 / 2 = 160
lda #PICTURE_HEIGHT
tax

; Calculate number of rows (BITMAP_HEIGHT) and store it into @mapHeight
lda #PICTURE_WIDTH
sta @mapWidth

@loopOuter:
    ldy @mapWidth  ; Load Y with mapHeight
    lda #$1
    sta @isFirstPixel
    lda @loopCounter
    @loopInner:
        lda @isFirstPixel
        bne @drawLeftBorder
        cpy #$1
        bne @default
        @drawRightBorder:
        lda #RIGHT_BORDER
        sta VERA_data0 ; Set a value that makes it obvious that this is the left border
        bra @continue
        @drawLeftBorder:
        lda #LEFT_BORDER
        sta VERA_data0 ; Set a value that makes it obvious that this is the right border
        bra @continue
        @default:
        lda #DEFAULT_BACKGROUND_COLOR
        sta VERA_data0  ; Store 0 into VRAM (set pixel to white)
        inc @loopCounter
        @continue:
        dey  ; Decrement Y
        stz @isFirstPixel       
        bne @loopInner  ; If Y is not 0, continue loop

    dex  ; Decrement X
    bne @loopOuter  ; If X is not 0, continue loop
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

b6InitGraphics:
jsr _b6DisableAndWaitForVsync

sei
lda #DISPLAY_SCALE
sta VERA_dc_hscale
sta VERA_dc_vscale

lda #^VRAM_palette | $10
sta VERA_addr_bank
lda #>VRAM_palette
sta VERA_addr_high
lda #<VRAM_palette
sta VERA_addr_low

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

lda #$6   ; Bitmap mode 16 colors
sta VERA_L0_config

jsr _b6InitBackground

jsr _b6InitCharset 

;Copy to vera here

stz ZP_TMP ;Reset to Zero so as to not break interpreter
stz ZP_TMP + 1

cli
rts

.endif