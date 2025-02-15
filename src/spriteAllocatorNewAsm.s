

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
.segment "BANKRAM0D"

bDSpriteAllocTable: .res TOTAL_REAL_BLOCKS, $0
bDSpriteAllocTableTerminator: .byte SPRITE_ALLOC_TERMINATOR

_bDBlocksBySizeFastLookup: .res FAST_LOOKUP_SIZE

.macro CALC_BLOCKS_TO_ALLOCATE
stx sreg
adc sreg
tax
lda _bDBlocksBySizeFastLookup,x
stp
sta BLOCKS_TO_FIND
.endmacro

.macro RESET_SPRITE_TABLE_POINTER
lda #>bDSpriteAllocTable
sta @lowByteLoop + 2
.endmacro

;void bDFindFreeVramBlock(SprSizes width, SprSizes height)
_bDFindFreeVramBlock:

stp

pha
jsr popa
plx

bDFindFreeVramBlockAsmCall:

CALC_BLOCKS_TO_ALLOCATE

lda #<TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER
lda #>TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER + 1

@highByteLoop:

ldx #$0
@lowByteLoop:
.repeat 4
lda bDSpriteAllocTable,x
bmi @handleTerminator

inx

.endrepeat

bne @lowByteLoop

@highByteCheckLoop:
dec BLOCKS_CHECKED_COUNTER + 1
bmi @end

inc @lowByteLoop + 2
bra @highByteLoop

@end:
stp
RESET_SPRITE_TABLE_POINTER

rts
@handleTerminator:
RESET_SPRITE_TABLE_POINTER
bra @highByteCheckLoop

.endif