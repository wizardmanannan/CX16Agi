.ifndef GARBAGE_INC

GARBAGE_INC = 1

.segment "BANKRAM0C"
_bCSpriteAddressReverseHighNotSet: .res 22, $0
_bCSpriteAddressReverseHighSet: .res $F8, $0

;void bCDeallocSpriteMemory(VeraSpriteAddress address)
_bCDeallocSpriteMemory:
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
stp
rts

.endif