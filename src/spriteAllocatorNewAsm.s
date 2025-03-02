

.ifndef SPRITE_MEMORY_MANAGER_NEW_INC
SPRITE_MEMORY_MANAGER_NEW_INC = 1
.importzp       sp

VRAM_START = $EA00        ;Base VRAM address
VRAM_SIZE = 69120         ;Total available VRAM for sprites
BLOCK_SIZE = 32           ;8x8 block size (64 bytes)
TOTAL_REAL_BLOCKS = (VRAM_SIZE / BLOCK_SIZE)  ;0x10E00 (2610)
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

;void _bDDeleteAllocation(VeraSpriteAddress address, SpriteAllocationSize width, SpriteAllocationSize height)
_bDDeleteAllocation:
sta sreg
jsr popa
ldx sreg

CALC_BLOCKS_TO_ALLOCATE

jsr popa
sta sreg + 1 ;low
jsr popax
stx sreg ;high
tax ;middle
lda sreg + 1


bDDeleteAllocationAsmCall:
jmp @start
@clearUnrolledInstructions:
    .word $0    ; Entry 0, 1
    .addr @1    ; Entry 0, 1
    .addr @2    ; Entry 2, 3
    .word 0     ; Entry 4, 5
    .addr @4    ; Entry 6, 7
    .word 0     ; Entry 8, 9
    .word 0     ; Entry 10, 11
    .word 0     ; Entry 12, 13
    .addr @8    ; Entry 14, 15
    .word 0     ; Entry 16, 17
    .word 0     ; Entry 18, 19
    .word 0     ; Entry 20, 21
    .word 0     ; Entry 22, 23
    .word 0     ; Entry 24, 25
    .word 0     ; Entry 26, 27
    .word 0     ; Entry 28, 29
    .addr @16   ; Entry 30, 31
    .word 0     ; Entry 32, 33
    .word 0     ; Entry 34, 35
    .word 0     ; Entry 36, 37
    .word 0     ; Entry 38, 39
    .word 0     ; Entry 40, 41
    .word 0     ; Entry 42, 43
    .word 0     ; Entry 44, 45
    .word 0     ; Entry 46, 47
    .word 0     ; Entry 48, 49
    .word 0     ; Entry 50, 51
    .word 0     ; Entry 52, 53
    .word 0     ; Entry 54, 55
    .word 0     ; Entry 56, 57
    .word 0     ; Entry 58, 59
    .word 0     ; Entry 60, 61
    .addr @32   ; Entry 62, 63
    .word 0     ; Entry 64, 65
    .word 0     ; Entry 66, 67
    .word 0     ; Entry 68, 69
    .word 0     ; Entry 70, 71
    .word 0     ; Entry 72, 73
    .word 0     ; Entry 74, 75
    .word 0     ; Entry 76, 77
    .word 0     ; Entry 78, 79
    .word 0     ; Entry 80, 81
    .word 0     ; Entry 82, 83
    .word 0     ; Entry 84, 85
    .word 0     ; Entry 86, 87
    .word 0     ; Entry 88, 89
    .word 0     ; Entry 90, 91
    .word 0     ; Entry 92, 93
    .word 0     ; Entry 94, 95
    .word 0     ; Entry 96, 97
    .word 0     ; Entry 98, 99
    .word 0     ; Entry 100, 101
    .word 0     ; Entry 102, 103
    .word 0     ; Entry 104, 105
    .word 0     ; Entry 106, 107
    .word 0     ; Entry 108, 109
    .word 0     ; Entry 110, 111
    .word 0     ; Entry 112, 113
    .word 0     ; Entry 114, 115
    .word 0     ; Entry 116, 117
    .word 0     ; Entry 118, 119
    .word 0     ; Entry 120, 121
    .word 0     ; Entry 122, 123
    .word 0   ; Entry 124, 125
    .addr @64   ; Entry 126, 127

;Width/Height: a/x Address y/sreg/sreg + 1
;y x sreg
@start:
sec
sbc #<SPRITE_START
tay
txa
sbc #>SPRITE_START
tax
lda sreg
sbc #$0
sta sreg

clc
lda sreg
lsr
txa
ror
tax
tya
ror
tay

.repeat 3
clc
txa
lsr
tax
tya
ror
tay
.endrepeat

clc
txa
lsr
tax
tya
ror

clc
adc #<_bDSpriteAllocTable
sta sreg
txa
adc #>_bDSpriteAllocTable
sta sreg + 1

lda BLOCKS_TO_FIND
cmp #16
bcs @largeDelete
jmp @smallDelete

@largeDelete:
asl
tax
lda #$0
ldy @clearUnrolledInstructions,x
sty sreg2
ldy @clearUnrolledInstructions + 1,x
sty sreg2 + 1
jmp (sreg2)

@64:
    ldy #63
    sta (sreg),y
    ldy #62
    sta (sreg),y
    ldy #61
    sta (sreg),y
    ldy #60
    sta (sreg),y
    ldy #59
    sta (sreg),y
    ldy #58
    sta (sreg),y
    ldy #57
    sta (sreg),y
    ldy #56
    sta (sreg),y
    ldy #55
    sta (sreg),y
    ldy #54
    sta (sreg),y
    ldy #53
    sta (sreg),y
    ldy #52
    sta (sreg),y
    ldy #51
    sta (sreg),y
    ldy #50
    sta (sreg),y
    ldy #49
    sta (sreg),y
    ldy #48
    sta (sreg),y

@32:
    ldy #31
    sta (sreg),y
    ldy #30
    sta (sreg),y
    ldy #29
    sta (sreg),y
    ldy #28
    sta (sreg),y
    ldy #27
    sta (sreg),y
    ldy #26
    sta (sreg),y
    ldy #25
    sta (sreg),y
    ldy #24
    sta (sreg),y
    ldy #23
    sta (sreg),y
    ldy #22
    sta (sreg),y
    ldy #21
    sta (sreg),y
    ldy #20
    sta (sreg),y
    ldy #19
    sta (sreg),y
    ldy #18
    sta (sreg),y
    ldy #17
    sta (sreg),y
    ldy #16
    sta (sreg),y

@16:
    ldy #15
    sta (sreg),y
    ldy #14
    sta (sreg),y
    ldy #13
    sta (sreg),y
    ldy #12
    sta (sreg),y
    ldy #11
    sta (sreg),y
    ldy #10
    sta (sreg),y
    ldy #9
    sta (sreg),y
    ldy #8
    sta (sreg),y

@8:
    ldy #7
    sta (sreg),y
    ldy #6
    sta (sreg),y
    ldy #5
    sta (sreg),y
    ldy #4
    sta (sreg),y

@4:
    ldy #3
    sta (sreg),y
    ldy #2
    sta (sreg),y

@2:
    ldy #1
    sta (sreg),y

@1:
    ldy #0
    sta (sreg),y
rts
@smallDelete:
tay
dey
lda #$0
@smallDeleteLoop:
sta (sreg),y
dey
bpl @smallDeleteLoop

rts

.endif