.ifndef VIEW_INC
VIEW_INC = 1

.import _offsetOfBackBuffers
.import _offsetOfIsOnBackBuffer
.import _viewTabNoToMetaData
.import _offsetOfMaxCels
.import _offsetOfViewMetadataBank
.import _offsetOfMaxVeraSlots
.import _offsetOfNumberOfLoops

.segment "BANKRAM0E"

;bESwitchMetadata(ViewTableMetadata* localMetadata, View* localView, byte entryNum)
LOCAL_VIEW = ZP_TMP_2
LOCAL_METADATA = ZP_TMP_3
ENTRY_NUM = ZP_TMP_4
_bESwitchMetadata:
.scope
sta ENTRY_NUM

jsr popax
sta LOCAL_VIEW
stx LOCAL_VIEW + 1

jsr popax
sta LOCAL_METADATA
stx LOCAL_METADATA + 1

lda #$0
ldx #$0
SET_STRUCT_16_STORED_OFFSET_VALUE_IN_REG _offsetOfBackBuffers, LOCAL_METADATA

lda #$0
SET_STRUCT_8_STORED_OFFSET_VALUE_IN_REG _offsetOfIsOnBackBuffer, LOCAL_METADATA

ldx ENTRY_NUM
lda #VIEWNO_TO_METADATA_NO_SET
sta _viewTabNoToMetaData,x

inc deadToBeCleared
ldx deadToBeCleared
GET_STRUCT_8_STORED_OFFSET_X_OFFSET_RESULT _offsetOfViewMetadataBank, LOCAL_METADATA, deadViewTableMetadataBank
GET_STRUCT_16_STORED_OFFSET_X_OFFSET_RESULT _offsetOfloopsVeraAddressesPointers, LOCAL_METADATA, deadLoopVeraAddressPointers
GET_STRUCT_16_STORED_OFFSET_X_OFFSET_RESULT _offsetOfBackBuffers, LOCAL_METADATA, deadBackBuffers
GET_STRUCT_8_STORED_OFFSET_X_OFFSET_RESULT _offsetOfNumberOfLoops, LOCAL_VIEW, deadNumOfLoops

GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxCels, LOCAL_VIEW
tax
GET_STRUCT_8_STORED_OFFSET  _offsetOfMaxVeraSlots, LOCAL_VIEW
ldy deadToBeCleared
sta  deadMaxVeraSlots,y
tay
txa
jsr  mul8x8to8               ; Multiply (maxCels * maxVeraSlots) => A
ldy deadToBeCleared
sta  deadMaxCels,y

rts
.endscope 

NOTHING_TO_BE_CLEARED = $FF

deadViewTableMetadataBank: .res VIEW_TABLE_SIZE
deadLoopVeraAddressPointers: .res VIEW_TABLE_SIZE * 2
deadBackBuffers: .res VIEW_TABLE_SIZE * 2
deadMaxCels: .res VIEW_TABLE_SIZE 
deadMaxVeraSlots: .res VIEW_TABLE_SIZE
deadNumOfLoops: .res VIEW_TABLE_SIZE
deadIsOnBackBuffer: .byte $0
deadToBeCleared: .byte NOTHING_TO_BE_CLEARED


_bEGarbageCollectSwitchedView:
ldx deadToBeCleared 
bmi @end

@deadToBeClearedLoop:
lda deadViewTableMetadataBank,x
sta SGC_LOOP_VERA_ADDR_BANK
lda deadLoopVeraAddressPointers,x
sta SGC_LOOP_VERA_ADDR
lda deadLoopVeraAddressPointers + 1,x
sta SGC_LOOP_VERA_ADDR + 1
lda deadBackBuffers,x
sta SGC_BACKBUFFERS
lda deadBackBuffers + 1,x
sta SGC_BACKBUFFERS + 1
lda deadMaxCels,x
sta SGC_MAX_CELS
lda deadMaxVeraSlots,x
sta SGC_MAX_VERA_SLOTS
lda deadNumOfLoops
sta SGC_NO_LOOPS

stp
phx
jsr deleteSpriteMemoryForViewTab
plx

@checkDeadToBeCleared:
dex
bpl @deadToBeClearedLoop

; sta SGC_LOOP_VERA_ADDR_BANK
; lda deadLoopVeraAddressPointers
; sta SGC_LOOP_VERA_ADDR
; lda deadLoopVeraAddressPointers 
; lda deadBackBuffers
; sta SGC_BACKBUFFERS

stx deadToBeCleared
@end:
rts

.endif

