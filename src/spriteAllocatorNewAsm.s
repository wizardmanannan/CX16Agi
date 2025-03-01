

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
stopBeingOptimistic = _bDSpriteAllocTable + TOTAL_REAL_BLOCKS - 64 - 1

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
sta findFreeVRamLowByteLoop + 1
lda #>_bDSpriteAllocTable
sta findFreeVRamLowByteLoop + 2
.endmacro

;void bDFindFreeVramBlock(SprSizes width, SprSizes height)
_bDFindFreeVramBlock:
pha
jsr popa
plx

bDFindFreeVramBlockAsmCall:
stp
CALC_BLOCKS_TO_ALLOCATE

ldx #$0
findFirstFreeVRamBlock_optimisticSkip:
bra findFirstFreeVRamBlock_occupy

ldy BLOCKS_TO_FIND
ldx #$0

lda #<TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER
lda #>TOTAL_BLOCKS
sta BLOCKS_CHECKED_COUNTER + 1
stz CONSECUTIVE_BLOCKS


findFirstFreeVRamBlock_highByteLoop:
ldx #$0

findFirstFreeVRamBlock_lowByteLoop_DebugPoint:
findFreeVRamLowByteLoop:
lda _bDSpriteAllocTable,x
bmi findFirstFreeVRamBlock_handleTerminator
bne findFirstFreeVRamBlock_occupied


findFirstFreeVRamBlock_notOccupied:
dey
bne findFirstFreeVRamBlock_incrementX

jmp findFirstFreeVRamBlock_occupy

findFirstFreeVRamBlock_occupied:
ldy BLOCKS_TO_FIND
findFirstFreeVRamBlock_incrementX:
inx

bne findFirstFreeVRamBlock_lowByteLoop_DebugPoint

findFirstFreeVRamBlock_highByteIncLoopCounter:
inc findFreeVRamLowByteLoop + 2

findFirstFreeVRamBlock_highByteCheckLoop:
dec BLOCKS_CHECKED_COUNTER + 1
beq findFirstFreeVRamBlock_endFail

bra findFirstFreeVRamBlock_highByteLoop

findFirstFreeVRamBlock_endFail:
stz sreg 
stz sreg + 1
lda #$0
ldx #$0

rts
findFirstFreeVRamBlock_handleTerminator:
RESET_SPRITE_TABLE_POINTER
stz CONSECUTIVE_BLOCKS

bra findFirstFreeVRamBlock_highByteCheckLoop

findFirstFreeVRamBlock_occupy:
clc
txa
adc findFreeVRamLowByteLoop + 1
sta findFreeVRamLowByteLoop + 1
sta findFirstFreeVRamBlock_occupyLoop + 1
lda #$0
adc findFreeVRamLowByteLoop + 2
sta findFreeVRamLowByteLoop + 2
sta findFirstFreeVRamBlock_occupyLoop + 2

lda BLOCKS_TO_FIND
dec
sta sreg

sec
lda findFirstFreeVRamBlock_occupyLoop + 1
sbc sreg
sta findFirstFreeVRamBlock_occupyLoop + 1
lda findFirstFreeVRamBlock_occupyLoop + 2
sbc #$0
sta findFirstFreeVRamBlock_occupyLoop + 2

lda #$1
ldy BLOCKS_TO_FIND
dey
findFirstFreeVRamBlock_occupyLoop:
sta _bDSpriteAllocTable,y
findFirstFreeVRamBlock_checkOccupyLoop:
dey
bpl findFirstFreeVRamBlock_occupyLoop

inc findFreeVRamLowByteLoop + 1
bne findFirstFreeVRamBlock_calculateAddress
inc findFreeVRamLowByteLoop + 2

findFirstFreeVRamBlock_calculateAddress:
sec
lda findFirstFreeVRamBlock_occupyLoop + 1
sbc #<_bDSpriteAllocTable
tay
lda findFirstFreeVRamBlock_occupyLoop + 2
sbc #>_bDSpriteAllocTable
tax

stz sreg
bne findFirstFreeVRamBlock_activateHigh
cpy #FIRST_THREE_BYTE_ALLOC_NUMBER
bcc findFirstFreeVRamBlock_multBy32

findFirstFreeVRamBlock_activateHigh:
inc sreg

findFirstFreeVRamBlock_multBy32:

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

ldy findFreeVRamLowByteLoop + 2
cpy #>stopBeingOptimistic
bcs findFreeVRamStopBeingOptimisticCheckAlreadyStopped
stp
rts

findFreeVRamStopBeingOptimisticCheckAlreadyStopped:
ldy findFirstFreeVRamBlock_optimisticSkip
cmp #NOP_IMP
bne findFreeVRamStopBeingOptimisticCheckLow
rts

findFreeVRamStopBeingOptimisticCheckLow:
ldy findFreeVRamLowByteLoop + 1
cpy #<stopBeingOptimistic
bcs findFreeVRamStopBeingOptimistic
rts

findFreeVRamStopBeingOptimistic:
ldy #NOP_IMP
sty findFirstFreeVRamBlock_optimisticSkip
sty findFirstFreeVRamBlock_optimisticSkip + 1

findFreeVRamOccupyReturn:
rts
findFreeVRamOccupyStopBeingOptimistic:
ldy #NOP_IMP
sty findFirstFreeVRamBlock_optimisticSkip
sty findFirstFreeVRamBlock_optimisticSkip + 1

rts

_bDResetSpriteTablePointer:
RESET_SPRITE_TABLE_POINTER

rts

_bDReenableOptimisticMode:
lda #BRA_ABS
sta findFirstFreeVRamBlock_optimisticSkip

lda #findFirstFreeVRamBlock_occupy - findFirstFreeVRamBlock_optimisticSkip
sta findFirstFreeVRamBlock_optimisticSkip
rts

.endif