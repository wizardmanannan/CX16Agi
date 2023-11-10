.ifndef SPRITE_MEMORY_MANAGER_INC
SPRITE_MEMORY_MANAGER_INC = 1

.include "globalGraphics.s"

.segment "BANKRAM0E"
SPRITE_ALLOC_TABLE_SIZE = (SPRITE_END - SPRITE_START) / SEGMENT_SMALL

_spriteAllocTable: .res SPRITE_ALLOC_TABLE_SIZE, $0

_bESpriteAddressTableMiddle: .res SPRITE_ALLOC_TABLE_SIZE, $0 ; Low will always be zero, hence no need for a table
_bESpriteHighByteStart: .byte $0

.endif