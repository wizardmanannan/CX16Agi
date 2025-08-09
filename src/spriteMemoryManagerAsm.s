.ifndef SPRITE_MEMORY_MANAGER_INC
SPRITE_MEMORY_MANAGER_INC = 1

.include "globalGraphics.s"
.include "globalViews.s"

.importzp sreg

.import popa

SPRITE_ALLOC_TABLE_SIZE = (SPRITE_END - SPRITE_START) / SEGMENT_SMALL
.segment "CODE"
_spriteAllocTable: .res SPRITE_ALLOC_TABLE_SIZE, $0

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
.endif