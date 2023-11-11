.ifndef SPRITE_MEMORY_MANAGER_INC
SPRITE_MEMORY_MANAGER_INC = 1

.include "globalGraphics.s"

.importzp sreg

.segment "BANKRAM0E"
SPRITE_ALLOC_TABLE_SIZE = (SPRITE_END - SPRITE_START) / SEGMENT_SMALL
RESULT_SIZE = 20

_bESpriteAllocTable: .res SPRITE_ALLOC_TABLE_SIZE, $0

_bESpriteAddressTableMiddle: .res SPRITE_ALLOC_TABLE_SIZE, $0 ; Low will always be zero, hence no need for a table
_bESpriteHighByteStart: .byte $0

_bE32SegmentPointer: .byte $0
_bE64SegmentPointer: .byte SPRITE_ALLOC_TABLE_SIZE - $1


.macro ALLOCATE_SPRITE_MEMORY_32
.local @loop
.local @found
.local @greater
.local @lowerBits
.local @nonEmpty
.local @return
.local @returnFail
ldy _bE32SegmentPointer
stz sreg
stz sreg + 1

stp
@loop:
lda _bESpriteAllocTable, y
bne @nonEmpty

@found:
lda #$1
sta _bESpriteAllocTable, y
cpy _bESpriteHighByteStart
bcs @greater

@lowerBits:
ldx _bESpriteAddressTableMiddle,y
lda #$0 ; Low byte is always zero
iny 
sty _bE32SegmentPointer
bra @return

@greater:
lda #$1
sta sreg
bra @lowerBits


@nonEmpty:
iny 
cpy _bE32SegmentPointer
beq @returnFail

cpy _bE64SegmentPointer
bne @loop

@resetAtZero:
ldy #$0
bra @loop

@returnFail:
lda #0
ldx #0
stz sreg
stz sreg + 1


@return:
sty _bE32SegmentPointer
.endmacro

;.scope ALLOCATE_SPR_MEM
;LOOP_COUNTER = ZP_TMP_14
_bEAllocateSpriteMemory:
ALLOCATE_SPRITE_MEMORY_32
rts
;.endscope

.endif