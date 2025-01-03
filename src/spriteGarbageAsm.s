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
.import _offsetOfFlags
.import _offsetOfBackBuffers
.import _offsetOfDirection
.import _offsetOfIsOnBackBuffer
.import _offsetOfDirection

.import _b5Multiply

.import _sizeOfViewTab
.import _sizeOfViewTableMetadata
.import _sizeOfView
.import _viewtab
.import _viewTableMetadata
.import _loadedViews

.import _bAViewTableQuickLookupLow
.import _bAViewTableQuickLookupHigh
.import _bAViewTableMetdataQuickLookupLow
.import _bAViewTableMetdataQuickLookupHigh
.import _bAViewQuickLookupLow
.import _bAViewQuickLookupHigh

.import _viewTabNoToMetaData

.segment "ZEROPAGE"
SGC_LOOP_VERA_ADDR: .res 2
SGC_CEL_VERA_ADDR: .res 2

VIEW_TAB_COUNTER: .res 1
LOOPS_COUNTER_ADDRESS: .res 1
CEL_COUNTER_ADDRESS: .res 1

;Used In By Sprite Garbage Collector. Uses some of the same ZPs are CelToVeraBulk due to lack of ZP space
SGC_VIEW_METADATA: .res 2
SGC_LOOP_VERA_ADDR_BANK: .res 1
SGC_CURRENT_LOOP: .res 1
SGC_CLEAR_ACTIVE_LOOP: .res 1
SGC_LOCAL_VIEW: .res 2
SGC_NO_LOOPS: .res 1
SGC_MAX_CELS: .res 1
SGC_VIEW_TAB: .res 2
SGC_SIZE_VIEW_TAB: .res 1
SGC_SIZE_VIEW_TAB_MD: .res 1
SGC_SIZE_VIEW: .res 1
SGC_FLAGS: .res 2
SGC_BACKBUFFERS: .res 2 ;Reusing this, as they should not be used at the same time
SGC_BACKBUFFER_VERA_ADDRESS: .res 2
SGC_MAX_VERA_SLOTS: .res 1
SGC_BACK_BUFFER_COUNTER: .res 1
.segment "CODE"

ANIMATED_AND_DRAWN = ANIMATED | DRAWN

;Ensure the the Zps are set up first
deleteSpriteMemoryForViewTab:
lda RAM_BANK
sta @previousRamBank

lda #VIEW_TAB_BANK
sta RAM_BANK
GET_STRUCT_8_STORED_OFFSET _offsetOfFlags, SGC_VIEW_TAB
and #ANIMATED_AND_DRAWN
sec
sbc #ANIMATED_AND_DRAWN
sta SGC_CLEAR_ACTIVE_LOOP
bne @initLoopsLoop

lda SGC_CURRENT_LOOP
asl
inc
sta SGC_CURRENT_LOOP


@initLoopsLoop:
lda SGC_NO_LOOPS
asl
dec
@loopsLoop:
tay
tax

lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK

lda (SGC_LOOP_VERA_ADDR),y
sta SGC_CEL_VERA_ADDR + 1
dey
lda (SGC_LOOP_VERA_ADDR),y
sta SGC_CEL_VERA_ADDR
dey
sty LOOPS_COUNTER_ADDRESS

lda SGC_CLEAR_ACTIVE_LOOP
bne @initCelsLoop
cpx SGC_CURRENT_LOOP
beq @checkForBackBuffer

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

@checkCelsLoop:
lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK
lda CEL_COUNTER_ADDRESS
bpl @celsLoop


@checkForBackBuffer:
lda #SPRITE_METADATA_BANK
sta RAM_BANK

GET_STRUCT_16_STORED_OFFSET _offsetOfBackBuffers, SGC_VIEW_METADATA, SGC_BACKBUFFERS  
ora SGC_BACKBUFFERS
beq @checkLoopsLoop

lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK

lda (SGC_BACKBUFFERS)
ldy #$1
ora (SGC_BACKBUFFERS),y
beq @checkLoopsLoop

lda SGC_BACKBUFFERS
jmp @checkIfBackBufferCanBeDeleted

@checkLoopsLoop:
lda LOOPS_COUNTER_ADDRESS
bpl @loopsLoop

lda @previousRamBank
sta RAM_BANK
rts
@previousRamBank: .byte $0

@checkIfBackBufferCanBeDeleted:

lda SGC_CLEAR_ACTIVE_LOOP
bne @initBackBufferLoop

lda SGC_CURRENT_LOOP
cmp LOOPS_COUNTER_ADDRESS
bne @initBackBufferLoop

lda SGC_FLAGS
and #MOTION
bne @checkLoopsLoop

GET_STRUCT_8_STORED_OFFSET _offsetOfIsOnBackBuffer, SGC_VIEW_METADATA
bne @checkLoopsLoop

lda #VIEW_TAB_BANK
sta RAM_BANK
GET_STRUCT_8_STORED_OFFSET _offsetOfDirection, SGC_VIEW_TAB
bne @checkLoopsLoop

@initBackBufferLoop:
lda SGC_MAX_VERA_SLOTS
asl
dec
tay
sta SGC_BACK_BUFFER_COUNTER
@backBufferLoop:
lda (SGC_BACKBUFFERS),y
sta sreg
dey
lda (SGC_BACKBUFFERS),y
ldx sreg

ldy #GARBAGE_BANK
sty RAM_BANK

jsr bADeallocSpriteMemory

lda SGC_LOOP_VERA_ADDR_BANK
sta RAM_BANK

ldy SGC_BACK_BUFFER_COUNTER
lda #$0
sta (SGC_BACKBUFFERS),y
dey
sta (SGC_BACKBUFFERS),y
dey

sty SGC_BACK_BUFFER_COUNTER

@checkBackbufferLoop:
bpl @backBufferLoop

jmp @checkLoopsLoop


START = 1
INCREMENT = 5
MINIMUM_GAP = 5 * SEGMENT_LARGE_SPACES
_runIncrementalGarbageCollector:
lda @nextStart
tax

sec ;If we have plenty of space free there is no need to run the collector
lda ZP_PTR_WALL_64
sbc ZP_PTR_WALL_32
cmp #MINIMUM_GAP + 1
bcs @end

stx sgc_startNo

lda @nextEnd
sta sgc_endNo


jsr runSpriteGarbageCollectorAsmStart

clc
lda @nextStart
adc #INCREMENT
sta @nextStart

lda @nextEnd
adc #INCREMENT
cmp #SPRITE_SLOTS

bcc @store
bne @reset
@decrement:
dec
@store:
sta @nextEnd
bra @end

@reset:
lda #START
sta @nextStart
lda #INCREMENT
sta @nextEnd

@end:
rts
@nextStart: .byte START
@nextEnd: .byte INCREMENT

;void runSpriteGarbageCollector(byte start, byte end)
_runSpriteGarbageCollector:
sta sgc_endNo
jsr popa
sta sgc_startNo

runSpriteGarbageCollectorAsmStart:
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

ldx sgc_endNo
stx VIEW_TAB_COUNTER

@viewTabLoop:
ldx VIEW_TAB_COUNTER
lda #VIEW_TAB_BANK
sta RAM_BANK
lda _bAViewTableQuickLookupLow,x
sta SGC_VIEW_TAB
lda _bAViewTableQuickLookupHigh,x
sta SGC_VIEW_TAB + 1

GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentLoop, SGC_VIEW_TAB, SGC_CURRENT_LOOP

lda #SPRITE_METADATA_BANK
sta RAM_BANK
lda _bAViewTableMetdataQuickLookupLow,x
sta SGC_VIEW_METADATA
lda _bAViewTableMetdataQuickLookupHigh,x
sta SGC_VIEW_METADATA + 1

GET_STRUCT_16_STORED_OFFSET _offsetOfloopsVeraAddressesPointers, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR
lda SGC_LOOP_VERA_ADDR 
ora SGC_LOOP_VERA_ADDR + 1
beq @incrementCounter

GET_STRUCT_8_STORED_OFFSET _offsetOfViewMetadataBank, SGC_VIEW_METADATA, SGC_LOOP_VERA_ADDR_BANK

lda #VIEW_TAB_BANK
sta RAM_BANK
GET_STRUCT_8_STORED_OFFSET _offsetOfCurrentView, SGC_VIEW_TAB

tay
lda #LOADED_VIEW_BANK
sta RAM_BANK
lda _bAViewQuickLookupLow,y
sta SGC_LOCAL_VIEW
lda _bAViewQuickLookupHigh,y
sta SGC_LOCAL_VIEW + 1
GET_STRUCT_16_STORED_OFFSET _offsetOfNumberOfLoops, SGC_LOCAL_VIEW, SGC_NO_LOOPS

GET_STRUCT_8_STORED_OFFSET _offsetOfMaxCels, SGC_LOCAL_VIEW
tax
GET_STRUCT_8_STORED_OFFSET _offsetOfMaxVeraSlots, SGC_LOCAL_VIEW
sta SGC_MAX_VERA_SLOTS
tay
txa
jsr mul8x8to8
sta SGC_MAX_CELS

jsr deleteSpriteMemoryForViewTab

@incrementCounter: 
lda VIEW_TAB_COUNTER
dec
cmp sgc_startNo
sta VIEW_TAB_COUNTER
bmi @endViewTabLoop
jmp @viewTabLoop

@endViewTabLoop:
pla 
sta RAM_BANK

rts
sgc_startNo: .byte $0
sgc_endNo: .byte $0

.segment "BANKRAM0A"
;void bARunSpriteGarbageCollectorAll()
_bARunSpriteGarbageCollectorAll:
lda VIEW_TABLE_SIZE - 1
sta sgc_endNo
lda #$0
sta sgc_endNo
jmp runSpriteGarbageCollectorAsmStart


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
beq @pushBackWall64 ;Don't push back if already at zero
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

;void bADeleteSpriteMemoryForViewTab(ViewTableMetadata* viewMetadata, byte currentLoop, View* localView, boolean clearActiveLoop)
_bADeleteSpriteMemoryForViewTab:
sta SGC_CLEAR_ACTIVE_LOOP

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