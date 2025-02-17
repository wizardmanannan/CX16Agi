

.ifndef SPRITE_MEMORY_MANAGER_NEW_INC
SPRITE_MEMORY_MANAGER_NEW_INC = 1

VRAM_START = $EA00        ;Base VRAM address
VRAM_SIZE = 69120         ;Total available VRAM for sprites
BLOCK_SIZE = 64           ;8x8 block size (64 bytes)
TOTAL_REAL_BLOCKS = (VRAM_SIZE / BLOCK_SIZE)
TOTAL_BLOCKS = 1280
SPRITE_ALLOC_TERMINATOR = $FF
FAST_LOOKUP_SIZE = 130
.segment "ZEROPAGE"
LAST_BLOCK_CHECKED: .byte $0
BLOCKS_CHECKED_COUNTER: .word $0
BLOCKS_TO_FIND: .byte $0
CONSECUTIVE_BLOCKS: .byte $0
.segment "BANKRAM0D"

bDSpriteAllocTable: .res TOTAL_REAL_BLOCKS, $0
bDSpriteAllocTableTerminator: .byte SPRITE_ALLOC_TERMINATOR

_bDBlocksBySizeFastLookup: .res FAST_LOOKUP_SIZE

.macro CALC_BLOCKS_TO_ALLOCATE
stx sreg
adc sreg
tax
lda _bDBlocksBySizeFastLookup,x
sta BLOCKS_TO_FIND
.endmacro

.macro RESET_SPRITE_TABLE_POINTER
lda #>bDSpriteAllocTable
sta @lowByteLoop + 2
.endmacro

;void bDFindFreeVramBlock(SprSizes width, SprSizes height)
_bDFindFreeVramBlock:
pha
jsr popa
plx

bDFindFreeVramBlockAsmCall:
stp

CALC_BLOCKS_TO_ALLOCATE
tay

lda #<TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER
lda #>TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER + 1
stz CONSECUTIVE_BLOCKS

@highByteLoop:

ldx #$0
@lowByteLoop:
lda bDSpriteAllocTable,x
bmi @handleTerminator

@notOccupied:
dey
bne @incrementX

jmp @occupy

@occupied:
ldy BLOCKS_TO_FIND
@incrementX:
inx


bne @lowByteLoop

@highByteCheckLoop:
dec BLOCKS_CHECKED_COUNTER + 1
bmi @endFail

inc @lowByteLoop + 2
bra @highByteLoop

@endFail:
lda #$0
ldx #$0

@endSuccess:

rts
@handleTerminator:
RESET_SPRITE_TABLE_POINTER
stz CONSECUTIVE_BLOCKS
bra @highByteCheckLoop

@occupy:

clc
txa
adc @lowByteLoop + 1
sta @lowByteLoop + 1
sta @occupyLoop + 1
lda #$0
adc @lowByteLoop + 2
sta @lowByteLoop + 2
sta @occupyLoop + 2

sec
lda @occupyLoop + 1
sbc BLOCKS_TO_FIND
sta @occupyLoop + 1
lda @occupyLoop + 2
sbc #$0
sta @occupyLoop + 2

lda #$1
ldy BLOCKS_TO_FIND
dey
@occupyLoop:
sta bDSpriteAllocTable,y
@checkOccupyLoop:
dey
bpl @occupyLoop

inc @lowByteLoop + 1
bne @calculateAddress
inc @lowByteLoop + 2

@calculateAddress:

stp
rts

.endif