.ifndef GARBAGE_INC

GARBAGE_INC = 1

.include "helpersAsm.s"

.import _offsetOfloopsVeraAddressesPointers
.import _offsetOfViewMetadataBank
.import _offsetOfNumberOfLoops
.import _offsetOfMaxCels
.import _offsetOfMaxVeraSlots
.import _offsetOfCurrentView
.import _offsetOfCurrentLoop
.import _b5Multiply

.import _sizeOfViewTab
.import _sizeOfViewTableMetadata
.import _sizeOfView
.import _viewtab
.import _viewTableMetadata
.import _loadedViews

.segment "CODE"

;Ensure the the Zps are set up first
deleteSpriteMemoryForViewTab:
lda RAM_BANK
sta @previousRamBank

lda SGC_INACTIVE_ONLY
beq @initLoopsLoop

lda SGC_CURRENT_LOOP
inc
asl
sec
sbc #3
sta SGC_CURRENT_LOOP


@initLoopsLoop:
lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK

lda SGC_NO_LOOPS
asl
dec
@loopsLoop:
tay
lda (SGC_LOOP_VERA_ADDR),y
sta SGC_CEL_VERA_ADDR + 1
dey
lda (SGC_LOOP_VERA_ADDR),y
sta SGC_CEL_VERA_ADDR
dey
sty LOOPS_COUNTER_ADDRESS

lda SGC_INACTIVE_ONLY
beq @initCelsLoop

cpy SGC_CURRENT_LOOP
beq @checkLoopsLoop

@initCelsLoop:
lda SGC_MAX_CELS
asl
dec
@celsLoop:
tay
lda (SGC_CEL_VERA_ADDR),y
tax
lda #$0
sta (SGC_CEL_VERA_ADDR),y
dey

lda (SGC_CEL_VERA_ADDR),y
sta sreg
lda #$0
sta (SGC_CEL_VERA_ADDR),y

dey
sty CEL_COUNTER_ADDRESS

txa ;If we have two zero bytes then there is nothing to do skip
ora sreg
beq @checkCelsLoop

lda sreg
ldy #GARBAGE_BANK
sty RAM_BANK
jsr bADeallocSpriteMemory
lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK

@checkCelsLoop:
lda CEL_COUNTER_ADDRESS
bpl @celsLoop


@checkLoopsLoop:
lda LOOPS_COUNTER_ADDRESS
bpl @loopsLoop

lda @previousRamBank
sta RAM_BANK
rts
@previousRamBank: .byte $0

;void runSpriteGarbageCollector(byte start, byte end)
_runSpriteGarbageCollector:
sta @end
jsr popa
sta @start

lda _sizeOfViewTab
sta SGC_SIZE_VIEW_TAB
lda _sizeOfViewTableMetadata
sta SGC_SIZE_VIEW_TAB_MD
lda _sizeOfView
sta SGC_SIZE_VIEW

lda RAM_BANK 
pha

lda #HELPERS_BANK
sta RAM_BANK

sec
lda @end
sbc @start
sta VIEW_TAB_COUNTER

stp
LOAD_FROM_16_ARRAY @start, #$0, SGC_SIZE_VIEW_TAB, _viewtab, SGC_VIEW_TAB
LOAD_FROM_16_ARRAY @start, #$0, SGC_SIZE_VIEW_TAB_MD, _viewTableMetadata, SGC_VIEW_METADATA

bra @viewTabLoop

@calculateNextViewTabAndMd:
clc
lda SGC_VIEW_TAB
adc SGC_SIZE_VIEW_TAB
sta SGC_VIEW_TAB
lda SGC_VIEW_TAB + 1
adc #$0
sta SGC_VIEW_TAB + 1

clc
lda SGC_VIEW_METADATA
adc SGC_SIZE_VIEW_TAB_MD
sta SGC_VIEW_METADATA
lda SGC_VIEW_METADATA + 1
adc #$0
sta SGC_VIEW_METADATA + 1

clc
lda SGC_LOCAL_VIEW
adc SGC_SIZE_VIEW
sta SGC_LOCAL_VIEW
lda SGC_LOCAL_VIEW + 1
adc #$0
sta SGC_LOCAL_VIEW + 1


@viewTabLoop:
;TODO: This a stop gap measure for loading view. As it is still using the old fashion manual memory management, (see view.c line 50) you can access it with the macro. Once we automatically assign in using BSS segments we can use the macro above
lda _loadedViews
sta sreg
lda _loadedViews + 1
sta sreg + 1

GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentView, SGC_VIEW_TAB
; ldx #$0
; jsr pushax
; lda SGC_SIZE_VIEW
; ldx #$0
; jsr _b5Multiply
; clc
; adc sreg
; sta SGC_LOCAL_VIEW
; txa
; adc sreg + 1
; sta SGC_LOCAL_VIEW + 1


lda #VIEW_TAB_BANK
sta RAM_BANK

GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentLoop, SGC_VIEW_TAB, SGC_CURRENT_LOOP

lda #SPRITE_METADATA_BANK
sta RAM_BANK
GET_STRUCT_16_STORED_OFFSET _offsetOfloopsVeraAddressesPointers, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR
GET_STRUCT_8_STORED_OFFSET _offsetOfViewMetadataBank, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR_BANK

lda #LOADED_VIEW_BANK
sta RAM_BANK
GET_STRUCT_16_STORED_OFFSET _offsetOfNumberOfLoops, SGC_LOCAL_VIEW, SGC_NO_LOOPS
GET_STRUCT_8_STORED_OFFSET _offsetOfMaxCels, SGC_LOCAL_VIEW
tay
GET_STRUCT_8_STORED_OFFSET _offsetOfMaxVeraSlots, SGC_LOCAL_VIEW
jsr mul8x8to8
sta SGC_MAX_CELS

jsr deleteSpriteMemoryForViewTab

dec VIEW_TAB_COUNTER
bpl @calculateNextViewTabAndMd
@endViewTabLoop:
pla 
sta RAM_BANK
rts
@start: .byte $0
@end: .byte $0

.segment "BANKRAM0A"

bADeallocSpriteMemory:
cpx #$0
beq @highNotSet

@highSet:
tay
ldx _bASpriteAddressReverseHighSet,y

bra @clearEntry

@highNotSet:
sec
sbc #SPRITE_START >> 8
tay
ldx _bASpriteAddressReverseHighNotSet,y 

@clearEntry:
stz _spriteAllocTable,x

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

rts

_bASpriteAddressReverseHighNotSet: .res 22, $0
_bASpriteAddressReverseHighSet: .res $F8, $0

;void bCDeleteSpriteMemoryForViewTab(ViewTableMetadata* viewMetadata, byte currentLoop, View* localView, boolean inActiveOnly)
_bADeleteSpriteMemoryForViewTab:
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

GET_STRUCT_8_STORED_OFFSET _offsetOfMaxCels, SGC_LOCAL_VIEW
tax
GET_STRUCT_8_STORED_OFFSET _offsetOfMaxVeraSlots, SGC_LOCAL_VIEW
tay
txa
jsr mul8x8to8
sta SGC_MAX_CELS

jsr deleteSpriteMemoryForViewTab
rts
.endif