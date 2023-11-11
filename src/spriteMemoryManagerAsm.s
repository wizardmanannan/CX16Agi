.ifndef SPRITE_MEMORY_MANAGER_INC
SPRITE_MEMORY_MANAGER_INC = 1

.include "globalGraphics.s"

.importzp sreg

.segment "BANKRAM0E"
SPRITE_ALLOC_TABLE_SIZE = (SPRITE_END - SPRITE_START) / SEGMENT_SMALL
RESULT_SIZE = 20

_bESpriteAllocTable: .res SPRITE_ALLOC_TABLE_SIZE, $0

_bESpriteAddressTableMiddle: .res SPRITE_ALLOC_TABLE_SIZE, $0 ; Low will always be zero, hence no need for a table

;low a middle x high y
.macro ALLOCATE_SPRITE_MEMORY_32
.local @loop
.local @found
.local @greater
.local @lesser
.local @nonEmpty
.local @return
.local @returnFail

ldy ZP_PTR_SEG_32

@loop:
cpy ZP_PTR_SEG_64
beq @resetAtZero
lda _bESpriteAllocTable, y
bne @nonEmpty

@found:
lda #$1
sta _bESpriteAllocTable, y

ldx _bESpriteAddressTableMiddle,y

tya 
iny 
sty ZP_PTR_SEG_32

cmp ZP_PTR_HIGH_BYTE_START
bcs @greater

@lesser:
ldy #$0
bra @return

@greater:
ldy #$1
bra @return


@nonEmpty:
iny 
cpy ZP_PTR_SEG_32
beq @returnFail

bra @loop

@resetAtZero:
cpy ZP_PTR_SEG_32 ;If we've looped back to the start, we've failed
beq @returnFail
ldy #$0
bra @loop

@returnFail:
sty ZP_PTR_SEG_32
ldy #0
ldx #0

@return:
lda #$0 ;Low byte is always zero
.endmacro

;.scope ALLOCATE_SPR_MEM
;LOOP_COUNTER = ZP_TMP_14
_bEAllocateSpriteMemory:
ALLOCATE_SPRITE_MEMORY_32
sty sreg
stz sreg + 1
rts
;.endscope

.endif