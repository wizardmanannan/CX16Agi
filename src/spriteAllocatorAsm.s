; SpriteMemoryManager.asm
; Manages allocation and deallocation of sprite VRAM blocks on the Commander X16

.ifndef SPRITE_MEMORY_MANAGER_NEW_INC       ; Include guard start
SPRITE_MEMORY_MANAGER_NEW_INC = 1           ; Define guard to prevent re-inclusion
.importzp       sp                          ; Use zero-page stack pointer register

VRAM_START = $EA00        ; Base VRAM address for sprite data
VRAM_SIZE = 69120         ; Total VRAM bytes available for sprites
BLOCK_SIZE = 32           ; Size of one 8×8 sprite block (bytes)
TOTAL_REAL_BLOCKS = (VRAM_SIZE / BLOCK_SIZE)  ; Total actual blocks we have to allocate 
TOTAL_BLOCKS = 2304       ; The number of block MUST be a multiple of 256, so this number is the total number of usable blocks rounded up to the next factor of 256.
SPRITE_ALLOC_TERMINATOR = $FF  ; Terminator value in allocation table.
FAST_LOOKUP_SIZE = 130     ; Fast lookup table length for block counts

.segment "ZEROPAGE"                        ; Zero-page variables for speed
LAST_BLOCK_CHECKED: .byte $0    ; Index of last block checked
BLOCKS_CHECKED_COUNTER: .word $0 ; Counter to limit search iterations
BLOCKS_TO_FIND: .byte $0        ; Number of contiguous blocks requested
CONSECUTIVE_BLOCKS: .byte $0     ; Counter of consecutive free blocks found
FIRST_THREE_BYTE_ALLOC_NUMBER = 176 ; Threshold for using 3-byte VRAM addresses



.segment "BANKRAM0E"
ZP_WIDTH = ZP_TMP_24 ;Starting from 3 as allocate functions use one tmp ZP
ZP_HEIGHT = ZP_TMP_24 + 1
ZP_NUMBER_TO_ALLOCATE = ZP_TMP_25
ZP_ARRAY_COUNTER = ZP_TMP_25 + 1
MAX_BULK_ALLOCATED_SIZE = 256
;Puts the returned memory addresses on the system stack. Note as the lower byte is always zero only the high and middle bytes (in that order) are pushed onto the stack
;boolean bEAllocateSpriteMemoryBulk(SpriteAllocationSize width, SpriteAllocationSize height, byte number);
_bEBulkAllocatedAddresses: .res MAX_BULK_ALLOCATED_SIZE, $0
_bEAllocateSpriteMemoryBulk:
sta ZP_NUMBER_TO_ALLOCATE ;Save the number of sprites to allocate
jsr popax
sta ZP_HEIGHT ;Save the size of the allocation
stx ZP_WIDTH

stz ZP_ARRAY_COUNTER

@loop:
lda ZP_WIDTH
ldx ZP_HEIGHT
TRAMPOLINE #$D, bDFindFreeVramBlock
ldy ZP_ARRAY_COUNTER
sta _bEBulkAllocatedAddresses,y

lda #$0
sta _bEBulkAllocatedAddresses + 3,y ;Forth byte is always zero, this means we can use a long in C to store the result

txa
sta _bEBulkAllocatedAddresses + 1,y 
lda sreg
sta _bEBulkAllocatedAddresses + 2,y

bne @incrementCounter ;If at least one byte is not zero we know that we have a result back
txa 
bne @incrementCounter
lda _bEBulkAllocatedAddresses,y
bne @incrementCounter
bra @returnFail ;If the high byte is zero and the middle byte is also zero then we have failed to allocate, due lack of memory

@incrementCounter:
iny
iny
iny
iny
sty ZP_ARRAY_COUNTER

dec ZP_NUMBER_TO_ALLOCATE
beq @return
jmp @loop


@return:
lda #$1
rts

@returnFail:
lda #$0
rts


.segment "BANKRAM0D"                      ; Main code segment in banked RAM

trap2: .byte $0                       ; Reserved trap byte

; Allocation table: one byte per block (0=free, 1=occupied)
_bDSpriteAllocTable: .res TOTAL_REAL_BLOCKS, $0  ; Initialize all to free
bDSpriteAllocTableTerminator: .byte SPRITE_ALLOC_TERMINATOR ; End marker
stopBeingOptimistic = _bDSpriteAllocTable + TOTAL_REAL_BLOCKS - 64 - 1 ; Optimistic skip threshold

_bDBlocksBySizeFastLookup: .res FAST_LOOKUP_SIZE ; Lookup table for sizes to block counts. The key is the width + height and the value is the number of blocks. No need to clear the carry when looking in this table because the value is duplicated in the address afterwards. See the bDBlocksBySizeFastLookup fast lookup in the C code 

; Macro: Calculate blocks needed based on size code in X (sreg)
.macro CALC_BLOCKS_TO_ALLOCATE
    stx sreg                     ; Save size code to sreg
    adc sreg                     ; Double size code (size*2)
    tax                          ; X = Width + Height
    lda _bDBlocksBySizeFastLookup, x ; Load block count
    sta BLOCKS_TO_FIND           ; Store needed blocks
.endmacro

; Macro: Reset pointer to start of allocation table
.macro RESET_SPRITE_TABLE_POINTER
    lda #<_bDSpriteAllocTable      ; Low byte of table start address
    sta findFreeVRamLowByteLoop + 1 ; Set low pointer
    lda #>_bDSpriteAllocTable      ; High byte
    sta findFreeVRamLowByteLoop + 2 ; Set high pointer
.endmacro

;C Label For Function below:
;unsigned long bDFindFreeVramBlock(SpriteAllocationSize width, SpriteAllocationSize height);
;Used only by tests transforms the arguments to be conformant to the assm label
_bDFindFreeVramBlock:
    pha                           ; Save A
    jsr popa                      ; Pull low return address byte into A
    plx                           ; Restore X

; Function: bDFindFreeVramBlock
; Finds first fit of contiguous free blocks and marks them occupied
; Input: X / sreg = width/height
; Output: A/X/sreg = VRAM block start address
; Optimistic mode is active by default when the memory hasn't being filled yet, we can freely allocate to the neighbouring block
bDFindFreeVramBlock:
    CALC_BLOCKS_TO_ALLOCATE       ; Determine BLOCKS_TO_FIND
    ldx BLOCKS_TO_FIND            ; Load blocks to find
    dex                           ; Zero-based count
    findFirstFreeVRamBlock_optimisticSkip: ;If we are in optimistic mode skip straight to allocate
     bra findFirstFreeVRamBlock_occupy ;This code is dynamically turned into NOP if we are not in optimistic mode

    ldy BLOCKS_TO_FIND            ; Initialize consecutive counter
    ldx #$0                        ; Reset X
    lda #<TOTAL_BLOCKS            ; Low byte of total blocks
    sta BLOCKS_CHECKED_COUNTER    ; Store search limit low
    lda #>TOTAL_BLOCKS            ; High byte
    sta BLOCKS_CHECKED_COUNTER + 1 ; Store high
    stz CONSECUTIVE_BLOCKS        ; Reset consecutive free blocks counter

findFirstFreeVRamBlock_highByteLoop:
    ldx #$0                        ; Start at table offset 0

findFirstFreeVRamBlock_lowByteLoop_DebugPoint:
findFreeVRamLowByteLoop:
    lda _bDSpriteAllocTable, x    ; Load table entry
    bmi findFirstFreeVRamBlock_handleTerminator ; If byte>=128 (terminator marker)
    bne findFirstFreeVRamBlock_occupied ; If non-zero, block occupied

findFirstFreeVRamBlock_notOccupied:
    dey                           ; Decrement needed count
    bne findFirstFreeVRamBlock_incrementX ; Keep scanning if more needed
    jmp findFirstFreeVRamBlock_occupy ; Found enough free blocks

findFirstFreeVRamBlock_occupied:
    ldy BLOCKS_TO_FIND            ; Restore blocks needed count

findFirstFreeVRamBlock_incrementX:
    inx                           ; Move to next entry
    bne findFirstFreeVRamBlock_lowByteLoop_DebugPoint ; Loop within low page

    ; End of low page: increment high pointer
    inc findFreeVRamLowByteLoop + 2

findFirstFreeVRamBlock_highByteCheckLoop:
    dec BLOCKS_CHECKED_COUNTER + 1 ; Decrement search limit high
    bmi findFirstFreeVRamBlock_endFail ; Fail if exhausted
    bra findFirstFreeVRamBlock_highByteLoop ; Continue scanning

findFirstFreeVRamBlock_endFail:
    stz sreg                      ; Return 0 on failure
    stz sreg + 1
    lda #$0
    ldx #$0
    rts                           ; Return

findFirstFreeVRamBlock_handleTerminator:
    ldy BLOCKS_TO_FIND            ; On terminator, restart scanning
    RESET_SPRITE_TABLE_POINTER    ; Reset pointer to table start
    stz CONSECUTIVE_BLOCKS        ; Reset free counter
    bra findFirstFreeVRamBlock_highByteCheckLoop ; Retry

; Mark the blocks as occupied and calculate VRAM address
findFirstFreeVRamBlock_occupy:
;Step 1: Calculate addresses for occupation and next search
    clc
    txa
    adc findFreeVRamLowByteLoop + 1 ; Add x to the to this value this will mean we start from here next time we search (Note we also need to add one as well that is done futher on otherwise we will be searching from the point we just allocated)
    sta findFreeVRamLowByteLoop + 1 ; Update pointer
    sta findFirstFreeVRamBlock_occupyLoop + 1 ;We use this when calculating the address of the LAST block we are allocating at the point of this label and it will be the same address as above
    lda #$0
    adc findFreeVRamLowByteLoop + 2 ; Compute high byte of table offset
    sta findFreeVRamLowByteLoop + 2
    sta findFirstFreeVRamBlock_occupyLoop + 2 ; Save for loop

    lda BLOCKS_TO_FIND            ; Blocks to occupy
    dec                           ; Count-1 for loop
    sta sreg                      ; Store in sreg

    sec                           ; Prepare for subtract
    lda findFirstFreeVRamBlock_occupyLoop + 1 ; Loop low pointer ;Go to the first block we are allocating by subtracting from the last block the number of blocks minus 1
    sbc sreg                      ; Subtract offset
    sta findFirstFreeVRamBlock_occupyLoop + 1 ; Adjust pointer to start
    lda findFirstFreeVRamBlock_occupyLoop + 2 ; High pointer
    sbc #$0
    sta findFirstFreeVRamBlock_occupyLoop + 2

    lda #$1                       ; Value to mark occupied ;Mark all of the blocks are occupied
    ldy BLOCKS_TO_FIND            ; Number of blocks
    dey                           ; Count-1

;Step 2: Occupy
findFirstFreeVRamBlock_occupyLoop:
    sta _bDSpriteAllocTable, y    ; Mark table entry
    dey
    bpl findFirstFreeVRamBlock_occupyLoop ; Loop

    inc findFreeVRamLowByteLoop + 1 ; Advance pointer past span
    bne findFirstFreeVRamBlock_calculateAddress ; If low didn't wrap
    inc findFreeVRamLowByteLoop + 2 ; Carry to high

;Step 3: Calculate VRAM Address
; Work out the block number with a subtraction. The allocated block address minus the address of the first block
findFirstFreeVRamBlock_calculateAddress:
    sec                           ; Compute block index
    lda findFirstFreeVRamBlock_occupyLoop + 1 ; Low index
    sbc #<_bDSpriteAllocTable    ; Subtract table base low
    tay                           ; Save to Y
    lda findFirstFreeVRamBlock_occupyLoop + 2 ; High index
    sbc #>_bDSpriteAllocTable    ;
    tax                           ;If the block number number address is 
    stz sreg               ; High byte of result = 0
    bne findFirstFreeVRamBlock_activateHigh ;We know that are the lowest three byte block number is 176 so therefore if we have have a second byte which is non zero (block number >= 256) the high byte must be set
    cpy #FIRST_THREE_BYTE_ALLOC_NUMBER
    bcc findFirstFreeVRamBlock_multBy32

    ; Multiply index (in X/Y) by BLOCK_SIZE=32 (shift left 5 times)
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
tya                          ; Return address in sreg:sreg+1

;Required by C to be set to zero as a long is four bytes but we only need 3.
stz sreg + 1

ldy findFreeVRamLowByteLoop + 2
cpy #>stopBeingOptimistic ;If the high byte of findFreeVRamLowByteLoop is less than the threshold then we can continue staying in optimistic mode 
bcs findFreeVRamStopBeingOptimisticCheckAlreadyStopped
rts

findFreeVRamStopBeingOptimisticCheckAlreadyStopped:
ldy findFirstFreeVRamBlock_optimisticSkip ;If optimistic mode is already turned off no need to waste cycles, return
cmp #NOP_IMP
bne findFreeVRamStopBeingOptimisticCheckLow
rts

findFreeVRamStopBeingOptimisticCheckLow:
ldy findFreeVRamLowByteLoop + 1 ;Now that we know that the high byte is greater than or equal to the threshold check the low byte
cpy #<stopBeingOptimistic
bcs findFreeVRamStopBeingOptimistic ;If it is less, return
rts

findFreeVRamStopBeingOptimistic:
ldy #NOP_IMP ;Replace the bra with no op (2 bytes)
sty findFirstFreeVRamBlock_optimisticSkip
sty findFirstFreeVRamBlock_optimisticSkip + 1

findFreeVRamOccupyReturn:
rts
_bDResetSpriteTablePointer:
    RESET_SPRITE_TABLE_POINTER    ; Reset scanning pointer
    rts

; Re-enable optimistic skipping logic
_bDReenableOptimisticMode:
    lda #BRA_ABS
    sta findFirstFreeVRamBlock_optimisticSkip ; Restore branch opcode
    lda #findFirstFreeVRamBlock_occupy - findFirstFreeVRamBlock_optimisticSkip - 2 ; Compute relative jump
    sta findFirstFreeVRamBlock_optimisticSkip + 1
    rts

; Function: _bDDeleteAllocation
; Frees previously allocated blocks by clearing table entries
_bDDeleteAllocation:
    sta sreg                      ; Save low byte of VRAM address
    jsr popa                      ; Pull mid-byte into A
    ldx sreg                      ; Save mid-byte in X

    CALC_BLOCKS_TO_ALLOCATE       ; Compute number of blocks to free

    jsr popa                      ; Discard extra
    sta sreg + 1                  ; Save low of block count
    jsr popax                     ; Pull high of block count into X
    stx sreg                      ; Save high
    tax                           ; Move mid to X
    lda sreg + 1                  ; Load low count
    tay                           ; Move to Y
    jsr popa                      ; Discard
    tya                           ; Restore Y

   bDDeleteAllocationAsmCall:
jmp @start                 ; Jump to clear routines

; Table of clear routines for large deletes (@1..@127 entries)
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


; php
; pha
; phx
; phy
; .import _trap
; lda _trap
; beq @continue
; stp
; @continue:
; ply
; plx
; pla
; plp


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
    ldy #48
    sta (sreg),y
    ldy #47
    sta (sreg),y
    ldy #46
    sta (sreg),y
    ldy #45
    sta (sreg),y
    ldy #44
    sta (sreg),y
    ldy #43
    sta (sreg),y
    ldy #42
    sta (sreg),y
    ldy #41
    sta (sreg),y
    ldy #40
    sta (sreg),y
    ldy #39
    sta (sreg),y
    ldy #38
    sta (sreg),y
    ldy #39
    sta (sreg),y
    ldy #38
    sta (sreg),y
    ldy #37
    sta (sreg),y
    ldy #36
    sta (sreg),y
    ldy #35
    sta (sreg),y
    ldy #34
    sta (sreg),y
    ldy #33
    sta (sreg),y
    ldy #32
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
    tay                           ; Y = block count
    dey                           ; Count-1
    lda #$0                        ; Clear value
@smallDeleteLoop:
    sta (sreg),y                 ; Clear table entry
    dey
    bpl @smallDeleteLoop
    rts

.endif                            ; End include guard
