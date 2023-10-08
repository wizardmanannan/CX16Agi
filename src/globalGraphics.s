.ifndef GLOBAL_GRAPHICS_INC
GLOBAL_GRAPHICS_INC = 1
; Vera Address

;BITMAP
BITMAP_WIDTH = 320
BITMAP_HEIGHT = 240
BYTES_PER_ROW = BITMAP_WIDTH / 2

PICTURE_WIDTH =  160  
PICTURE_HEIGHT =  168

STARTING_ROW = (BITMAP_HEIGHT / 2) - (PICTURE_HEIGHT / 2)
STARTING_BYTE = STARTING_ROW * BYTES_PER_ROW

;Tile Layer

TILE_BASE = $D000
DEFAULT_BACKGROUND_COLOR = $FF
LEFT_BORDER = $F0
RIGHT_BORDER = $0F

BYTES_PER_CHARACTER = 16
TRANSPARENT_CHAR = $AF
NO_CHARS = 160
SIZE_OF_CHARSET = (BYTES_PER_CHARACTER * NO_CHARS)

TILE_LAYER_WIDTH = 64
TILE_LAYER_HEIGHT = 32

.macro SET_VERA_ADDRESS_ABSOLUTE VeraAddress, AddressSel, Stride ;Vera Address is a 4 bit number instead of three to make it easier to work with C
        lda AddressSel
        sta VERA_ctrl  
        lda VeraAddress + 2 ; Ignore plus 3
        and #$1 ; We only care about the first bit
        ora Stride << 4
        sta VERA_addr_bank
        lda VeraAddress
        sta VERA_addr_low
        lda VeraAddress + 1
        sta VERA_addr_high
.endmacro

.endif