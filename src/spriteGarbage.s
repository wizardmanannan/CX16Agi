.ifndef GARBAGE_INC

GARBAGE_INC = 1

.import _offsetOfloopsVeraAddressesPointers
.import _offsetOfViewMetadataBank
.import _offsetOfNumberOfLoops
.import _offsetOfMaxCels

.macro DEALLOC_SPRITE_MEMORY
.local @highSet
.local @highNotSet
.local @clearEntry
.local @pushBackWall32
.local @pushBackWall64
.local @end

stp

cpx #$0
beq @highNotSet

@highSet:
tay
ldx _bCSpriteAddressReverseHighSet,y

bra @clearEntry

@highNotSet:
sec
sbc #SPRITE_START >> 8
tay
ldx _bCSpriteAddressReverseHighNotSet,y 

@clearEntry:
stz _bESpriteAllocTable,x

@pushBackWall32:
txa 
beq @pushBackWall32 ;Don't push back if already at zero
cpx ZP_PTR_WALL_32
bne @pushBackWall64
dec ZP_PTR_WALL_32

@pushBackWall64:
cpx #SPRITE_ALLOC_TABLE_SIZE - SEGMENT_LARGE_SPACES ;Don't push back if already at end
beq @end
cpx ZP_PTR_WALL_64
bne @end

clc
lda ZP_PTR_WALL_64
adc #SEGMENT_LARGE_SPACES
sta ZP_PTR_WALL_64

@end:

.endmacro

.segment "CODE"

;void deleteSpriteMemoryForViewTab(ViewTab* viewTab, byte currentView boolean inActiveOnly)
_deleteSpriteMemoryForViewTab:
sta SGC_INACTIVE_ONLY
jsr popax
sta SGC_VIEW_METADATA
stx SGC_VIEW_METADATA + 1

.segment "BANKRAM0C"
_bCSpriteAddressReverseHighNotSet: .res 22, $0
_bCSpriteAddressReverseHighSet: .res $F8, $0

;void bCDeleteSpriteMemoryForViewTab(ViewTableMetadata* viewMetadata, byte currentLoop, View* localView, boolean inActiveOnly)
_bCDeleteSpriteMemoryForViewTab:
stp
sta SGC_INACTIVE_ONLY

jsr popax 
sta SGC_LOCAL_VIEW
stx SGC_LOCAL_VIEW + 1

jsr popa
sta SGC_CURRENT_LOOP

jsr popax
sta SGC_VIEW_METADATA
stx SGC_VIEW_METADATA + 1


GET_STRUCT_16_STORED_OFFSET _offsetOfloopsVeraAddressesPointers, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR
GET_STRUCT_8_STORED_OFFSET _offsetOfViewMetadataBank, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR_BANK
GET_STRUCT_16_STORED_OFFSET _offsetOfNumberOfLoops, SGC_LOCAL_VIEW, SGC_NO_LOOPS
GET_STRUCT_8_STORED_OFFSET _offsetOfMaxCels, SGC_LOCAL_VIEW, SGC_MAX_CELS

stp

rts
.endif