.ifndef GRAPHICS_INC
; Set the value of include guard and define constants
GRAPHICS_INC = 1

.include "global.s"
.include "globalGraphics.s"
.include "helpersAsm.s"
.include "irqAsm.s"
.include "lineDrawing.s"

PRIORITY_SLICE_SIZE = 2048
PRIORITY_TAIL_SIZE = PRIORITY_SIZE .mod PRIORITY_SLICE_SIZE

.segment "BANKRAM06"
.import _b3InitCharset
.import pushax
.import pusha
.import popa
.import _b6DisplayLoadingScreen
.import _b6InitVeraMemory
.import _bAInitSpriteData
.import _b3SetTextColor
.import _inputLineDirty
.importzp ptr4

.macro SET_VERA_ON_PALETTE
stz VERA_ctrl
lda #^VRAM_palette | $10
sta VERA_addr_bank
lda #>VRAM_palette
sta VERA_addr_high
lda #<VRAM_palette
sta VERA_addr_low
.endmacro

_b6Clear:
lda #TILE_BYTE_2
TRAMPOLINE #TEXT_BANK, _b3InitLayer1Mapbase
TRAMPOLINE #SPRITE_UPDATES_BANK, _bEClearSpriteAttributes
TRAMPOLINE #PICTURE_CODE_OVERFLOW_BANK, _b8ClearPicture
TRAMPOLINE #GRAPHICS_BANK, _b6InitInput
rts

_b6BackgroundColorToSet: .word $0 ;The color where there is no bitmap or tile over the top, not the bitmap background
_b6SetBackgroundColor:
SET_VERA_ON_PALETTE

lda _b6BackgroundColorToSet
sta VERA_data0
lda _b6BackgroundColorToSet + 1
sta VERA_data0

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

; Protect the shared sprite state and VERA setup until the display is ready.
jsr _b6EnterForegroundVera
jsr _b6InitVeraMemory
TRAMPOLINE #SPRITE_INIT_BANK, _bAInitSpriteData

lda #DISPLAY_SCALE_GRAPHICS
sta VERA_dc_hscale
sta VERA_dc_vscale

SET_VERA_ON_PALETTE

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
lda #<COLOR_BLACK ;White background black text
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
lda #<COLOR_BLACK ;White text black background
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

;TileSet Layer 3
lda #<COLOR_BLACK
sta VERA_data0
lda #>COLOR_BLACK
sta VERA_data0

lda #<COLOR_WHITE
sta VERA_data0
lda #>COLOR_WHITE
sta VERA_data0

lda #<COLOR_LIGHT_GRAY
sta VERA_data0
lda #>COLOR_LIGHT_GRAY
sta VERA_data0

lda #<COLOR_RED
sta VERA_data0
lda #>COLOR_RED
sta VERA_data0


;Other palettes are dynamic created in paletteManager.c

lda #$6   ; Bitmap mode 16 colors
sta VERA_L0_config
stz VERA_L0_tilebase ;A 320 * 240 pixel bitmap at the beginning of VRAM

TRAMPOLINE #LINE_DRAWING_BANK, _b8SetupLineTables
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
lda #TILE_BYTE_2
TRAMPOLINE #TEXT_BANK, _b3InitLayer1Mapbase
TRAMPOLINE #SPRITE_UPDATES_BANK, _bEClearSpriteAttributes 
jsr _b6InitInput

lda #DEFAULT_TEXT_FOREGROUND
ldx #$0
jsr pushax

lda #DEFAULT_TEXT_BACKGROUND
ldx #$0

jsr _b6LeaveForegroundVera

TRAMPOLINE #TEXT_BANK, _b3SetTextColor 



lda #IRQ_CMD_NORMAL
ldx #$0
jsr _b6SetAndWaitForIrqStateAsm

rts
@vSyncToCheck: .word $0

_b6InitInput:
SET_VERA_ADDRESS_IMMEDIATE_SPLIT #<INPUT_STRING_ADDRESS + 1, #>INPUT_STRING_ADDRESS, #$0, #$0, #$2 ;To to the first tile byte 1
ldx #INPUT_BYTE_1
lda #INPUT_LINE_RENDER_CELLS

@initLoop:
stx VERA_data0

@checkCondition:
dec
bne @initLoop
lda #$1
sta _inputLineDirty
rts

.segment "BANKRAM08"
_b8ClearPicture:
 jsr _b8ClearBackground
 jsr _b8ClearPriority
 rts

_b8ClearPriority:
; Cache groups require aligned starts; direct byte tails support any exact byte count.
.assert (PRIORITY_START .mod 4) = 0, error, "Priority start must be four-byte aligned"
.assert (PRIORITY_SLICE_SIZE .mod 4) = 0, error, "Priority slice must be a multiple of four bytes"
.assert PRIORITY_SIZE <= $FFFF, error, "Priority byte count must fit in 16 bits"
.assert PRIORITY_START + PRIORITY_SIZE < $10000, error, "Priority cursor must stay below 64 KiB"
.assert (PRIORITY_SLICE_SIZE .mod 256) = 0, error, "Priority slice address advance must fit in one high-byte addition"

stz VERA_ctrl
lda #$10 | ^PRIORITY_START
sta VERA_addr_bank
lda #> PRIORITY_START
sta VERA_addr_high
lda #< PRIORITY_START
sta VERA_addr_low

ldx #<(PRIORITY_SIZE / 16)
ldy #>(PRIORITY_SIZE / 16)
jsr b8PriorityCacheFill
.if (PRIORITY_SIZE .mod 16) <> 0
lda #PRIORITY_DEF
.repeat (PRIORITY_SIZE .mod 16)
sta VERA_data0
.endrepeat
.endif
rts

; IRQ clear uses row-aligned slices so every invocation stays below 2048 writes.
_b8ClearBackgroundSliceReset:
lda #<STARTING_BYTE
sta b8ClearBackgroundAddress
lda #>STARTING_BYTE
sta b8ClearBackgroundAddress + 1
lda #PICTURE_HEIGHT
sta b8ClearBackgroundRowsRemaining
rts

_b8ClearBackgroundSlice:
stz VERA_ctrl
lda #$10 | ^STARTING_BYTE
sta VERA_addr_bank
lda b8ClearBackgroundAddress + 1
sta VERA_addr_high
lda b8ClearBackgroundAddress
sta VERA_addr_low
lda #12
sta b8ClearBackgroundRowsThisSlice

@backgroundRow:
lda #LEFT_BORDER
sta VERA_data0
lda #DEFAULT_BACKGROUND_COLOR
ldy #((PICTURE_WIDTH - 2) / 8)
@backgroundFill:
    .repeat 8
    sta VERA_data0
    .endrepeat
    dey
    bne @backgroundFill
.repeat ((PICTURE_WIDTH - 2) .mod 8)
sta VERA_data0
.endrepeat
lda #RIGHT_BORDER
sta VERA_data0

dec b8ClearBackgroundRowsRemaining
beq @backgroundComplete
dec b8ClearBackgroundRowsThisSlice
bne @backgroundRow

clc
lda b8ClearBackgroundAddress
adc #<(12 * BYTES_PER_ROW)
sta b8ClearBackgroundAddress
lda b8ClearBackgroundAddress + 1
adc #>(12 * BYTES_PER_ROW)
sta b8ClearBackgroundAddress + 1
lda #$0
rts

@backgroundComplete:
lda #$1
rts

b8ClearBackgroundAddress: .word STARTING_BYTE
b8ClearBackgroundRowsRemaining: .byte PICTURE_HEIGHT
b8ClearBackgroundRowsThisSlice: .byte $0

; Clear 2048 priority bytes per call; the final slice emits the exact remainder.
_b8ClearPrioritySliceReset:
lda #<PRIORITY_START
sta b8ClearPriorityAddress
lda #>PRIORITY_START
sta b8ClearPriorityAddress + 1
lda #<PRIORITY_SIZE
sta b8ClearPriorityRemaining
lda #>PRIORITY_SIZE
sta b8ClearPriorityRemaining + 1
rts

_b8ClearPrioritySlice:
stz VERA_ctrl
lda #$10 | ^PRIORITY_START
sta VERA_addr_bank
lda b8ClearPriorityAddress + 1
sta VERA_addr_high
lda b8ClearPriorityAddress
sta VERA_addr_low
ldx b8ClearPriorityRemaining + 1
cpx #>PRIORITY_SLICE_SIZE
bcc @priorityTail

ldx #<(PRIORITY_SLICE_SIZE / 16)
ldy #>(PRIORITY_SLICE_SIZE / 16)
jsr b8PriorityCacheFill

sec
lda b8ClearPriorityRemaining + 1
sbc #>PRIORITY_SLICE_SIZE
sta b8ClearPriorityRemaining + 1
clc
lda b8ClearPriorityAddress + 1
adc #>PRIORITY_SLICE_SIZE
sta b8ClearPriorityAddress + 1
lda b8ClearPriorityRemaining
ora b8ClearPriorityRemaining + 1
beq @priorityComplete
lda #$0
rts

@priorityTail:
ldx #<(PRIORITY_TAIL_SIZE / 16)
ldy #>(PRIORITY_TAIL_SIZE / 16)
jsr b8PriorityCacheFill
.if (PRIORITY_TAIL_SIZE .mod 16) <> 0
lda #PRIORITY_DEF
.repeat (PRIORITY_TAIL_SIZE .mod 16)
sta VERA_data0
.endrepeat
.endif

@priorityComplete:
lda #$1
rts

b8ClearPriorityAddress: .word PRIORITY_START
b8ClearPriorityRemaining: .word PRIORITY_SIZE

b8PriorityCacheFill:
lda #$0C
sta VERA_ctrl
lda #PRIORITY_DEF
sta $9F29
sta $9F2A
sta $9F2B
sta $9F2C
lda #$30 | ^PRIORITY_START
sta VERA_addr_bank
lda #$04
sta VERA_ctrl
lda #$40
sta VERA_dc_video

; X/Y contain 16-byte cache groups; exact byte remainders use ordinary writes.
cpx #$00
beq @priorityGroupCount
iny ; The nested 8-bit counter includes a partial low-byte run.
@priorityGroupCount:
cpy #$00
beq @priorityRemainder
@priorityFullGroup:
.repeat 4
stz VERA_data0
.endrepeat
dex
bne @priorityFullGroup
dey
bne @priorityFullGroup
@priorityRemainder:
lda #$04
sta VERA_ctrl
stz VERA_dc_video
stz VERA_ctrl
lda #$10 | ^PRIORITY_START
sta VERA_addr_bank
rts

_b8ClearBackground:
stz VERA_ctrl
lda #$10 | ^STARTING_BYTE
sta VERA_addr_bank
lda #> (STARTING_ROW * (BITMAP_WIDTH / 2) ) ;There are 320 bytes per row, but since each pixel is 4 bits we divide by 2
sta VERA_addr_high
lda #< (STARTING_ROW * (BITMAP_WIDTH / 2) )
sta VERA_addr_low

; Write each border once per row and unroll the 158-byte interior fill.
lda #PICTURE_HEIGHT
tax

@loopOuter:
lda #LEFT_BORDER
sta VERA_data0
lda #DEFAULT_BACKGROUND_COLOR
ldy #((PICTURE_WIDTH - 2) / 8)
@loopInner:
    .repeat 8
    sta VERA_data0
    .endrepeat
    dey
    bne @loopInner  ; If Y is not 0, continue loop
.repeat ((PICTURE_WIDTH - 2) .mod 8)
sta VERA_data0
.endrepeat
lda #RIGHT_BORDER
sta VERA_data0
dex
bne @loopOuter
rts

.endif
