

.ifndef SPRITE_MEMORY_MANAGER_NEW_INC
SPRITE_MEMORY_MANAGER_NEW_INC = 1

VRAM_START = $EA00        ;Base VRAM address
VRAM_SIZE = 69120         ;Total available VRAM for sprites
BLOCK_SIZE = 32           ;8x8 block size (64 bytes)
TOTAL_REAL_BLOCKS = (VRAM_SIZE / BLOCK_SIZE)
TOTAL_BLOCKS = 2304
SPRITE_ALLOC_TERMINATOR = $FF
FAST_LOOKUP_SIZE = 130
.segment "ZEROPAGE"
LAST_BLOCK_CHECKED: .byte $0
BLOCKS_CHECKED_COUNTER: .word $0
BLOCKS_TO_FIND: .byte $0
CONSECUTIVE_BLOCKS: .byte $0
FIRST_THREE_BYTE_ALLOC_NUMBER = 176
.segment "BANKRAM0D"

_bDSpriteAllocTable: .res TOTAL_REAL_BLOCKS, $0
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
lda #<_bDSpriteAllocTable
sta @lowByteLoop + 1
lda #>_bDSpriteAllocTable
sta @lowByteLoop + 2
.endmacro

;void bDFindFreeVramBlock(SprSizes width, SprSizes height)
_bDFindFreeVramBlock:
pha
jsr popa
plx

bDFindFreeVramBlockAsmCall:
CALC_BLOCKS_TO_ALLOCATE
tay

lda #<TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER
lda #>TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER + 1
stz CONSECUTIVE_BLOCKS

@highByteLoop:
ldx #$0

@lowByteLoop_DebugPoint:
@lowByteLoop:
lda _bDSpriteAllocTable,x
bmi @handleTerminator
bne @occupied


@notOccupied:
dey
bne @incrementX

jmp @occupy

@occupied:
ldy BLOCKS_TO_FIND
@incrementX:
inx

bne @lowByteLoop_DebugPoint

@highByteIncLoopCounter:
inc @lowByteLoop + 2

@highByteCheckLoop:

php
pha
phx
phy
.import _trap
lda _trap
beq @continue
@continue:
ply
plx
pla
plp

dec BLOCKS_CHECKED_COUNTER + 1
beq @endFail

bra @highByteLoop

@endFail:
stz sreg 
stz sreg + 1
lda #$0
ldx #$0

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

lda BLOCKS_TO_FIND
dec
sta sreg

sec
lda @occupyLoop + 1
sbc sreg
sta @occupyLoop + 1
lda @occupyLoop + 2
sbc #$0
sta @occupyLoop + 2

lda #$1
ldy BLOCKS_TO_FIND
dey
@occupyLoop:
sta _bDSpriteAllocTable,y
@checkOccupyLoop:
dey
bpl @occupyLoop

inc @lowByteLoop + 1
bne @calculateAddress
inc @lowByteLoop + 2

@calculateAddress:
sec
lda @occupyLoop + 1
sbc #<_bDSpriteAllocTable
tay
lda @occupyLoop + 2
sbc #>_bDSpriteAllocTable
tax

stz sreg
bne @activateHigh
cpy #FIRST_THREE_BYTE_ALLOC_NUMBER
bcc @multBy32

@activateHigh:
inc sreg

@multBy32:

.repeat 5
tya
asl
tay
txa
rol
tax
.endrepeat

clc
tya 
adc #<SPRITE_START
tay
txa
adc #>SPRITE_START
tax
tya

stz sreg + 1
rts

.endif