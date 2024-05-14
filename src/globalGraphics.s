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
DEFAULT_BACKGROUND_COLOR = $FF

;Tile Layer

DEFAULT_TEXT_BACKGROUND = $0
DEFAULT_TEXT_FOREGROUND = $F
TILE_BASE = $D000
MAP_BASE = $DA00
LEFT_BORDER = $F0
RIGHT_BORDER = $0F

BYTES_PER_CHARACTER = 16 ; Titlebase
TRANSPARENT_CHAR = $80
NO_CHARS = 160
SIZE_OF_CHARSET = (BYTES_PER_CHARACTER * NO_CHARS)

TILE_LAYER_WIDTH = 64
TILE_LAYER_HEIGHT = 32
TILE_LAYER_NO_TILES = (TILE_LAYER_WIDTH * TILE_LAYER_HEIGHT)
TILE_BYTE_2 = $10

FIRST_ROW = 4

BYTES_PER_CELL = 2 ;Mapbase
TILE_LAYER_BYTES_PER_ROW = (BYTES_PER_CELL * TILE_LAYER_WIDTH)

;Sprite Layer
SPRITE_START = $EA00
SPRITE_END = $1F9BE
SPRITES_PIXELS_PER_BYTE = $2

VERA_ADDRESS_SIZE = $3
VERA_SPRITE_ADDRESS_SIZE = $2
SPRITE_TOTAL_BYTES_32 = 512
SPRITE_TOTAL_BYTES_64 = 2048

BYTES_PER_ROW_8 = 4
BYTES_PER_ROW_16 = 8
BYTES_PER_ROW_32 = 16
BYTES_PER_ROW_64 = 32

;Sprite Attribute Size
SPR_ATTR_8 = 0
SPR_ATTR_16 = 1
SPR_ATTR_32 = 2
SPR_ATTR_64 = 3


SPRITE_TOTAL_ROWS_8 = 8
SPRITE_TOTAL_ROWS_16 = 16
SPRITE_TOTAL_ROWS_32 = 32
SPRITE_TOTAL_ROWS_64 = 64
SPRITE_ATTR_START = $1FC00
SPRITE_ATTR_SIZE = 8

SPRITE_SLOTS = VIEW_TABLE_SIZE
MAX_SPRITES_SLOTS_PER_VIEW_TAB = 6
MAX_SPRITE_SLOTS = (MAX_SPRITES_SLOTS_PER_VIEW_TAB * SPRITE_SLOTS)

BLACK_COLOR = $0

;Sprite Attributes
SA_VERA_ADDRESS_LOW = $0
SA_VERA_ZORDER = $6
SA_VERA_TOTAL = $8
NO_SPRITES = 127 ;Not including the mouse at 0


;Sprite Memory Manager
SEGMENT_SMALL = (32 * 32) / 2
SEGMENT_LARGE = SEGMENT_SMALL * 2 * SEGMENT_SMALL * 2


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

.macro SET_VERA_ADDRESS address, stride, highbyte, ctrl
.ifnblank stride
lda stride << 4
.endif
.ifblank stride
lda #$10 
.endif
sta VERA_addr_bank 

.ifnblank highbyte
lda highbyte
and #$1 ; We only care about the first bit
ora VERA_addr_bank
sta VERA_addr_bank
.endif

.ifblank ctrl
stz VERA_ctrl
.endif
.ifnblank ctrl
lda ctrl
and #$1
ora VERA_ctrl
sta VERA_ctrl
.endif

lda address + 1
sta VERA_addr_high

lda address
sta VERA_addr_low

.endmacro


.macro SET_VERA_ADDRESS_IMMEDIATE VERA_ADDRESS, ADDRESS_SEL, STRIDE ;Vera Address is a 4 bit number instead of three to make it easier to work with C
        lda ADDRESS_SEL
        sta VERA_ctrl  
        lda #^VERA_ADDRESS              
        and #$1 ; We only care about the first bit
        ora STRIDE << 4
        sta VERA_addr_bank
        lda #<VERA_ADDRESS
        sta VERA_addr_low
        lda #>VERA_ADDRESS
        sta VERA_addr_high
.endmacro

.macro CLEAR_VERA VERA_ADDRESS, NO_ROWS, BYTES_PER_ROW, COLOUR
.local @loop
.local @loopCheck
SET_VERA_ADDRESS_ABSOLUTE VERA_ADDRESS, #$0, #$1

lda COLOUR
SET_COLOR_LEFT COLOUR

ldx NO_ROWS
@loopOuter:

ldy BYTES_PER_ROW
@loopInner:
sta VERA_data0

@loopInnerCheck:
dey
bne @loopInner

@loopOuterCheck:
dex
bne @loopOuter

.endmacro

.endif