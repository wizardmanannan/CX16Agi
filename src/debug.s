.ifndef DEBUG_INC
DEBUG_INC = 1
.include "codeWindow.s"
.include "irqAsm.s"
.include "fillAsm.s"
.include "spriteGarbageAsm.s"
.segment "BANKRAM06"
.import _opCounter
.import _pixelCounter
_b6TellMeTheAddressPlease:
lda _opCounter
lda _bESpritesUpdatedBuffer
lda _bASpriteAddressReverseHighNotSet
lda _bASpriteAddressReverseHighSet
rts

.segment "BANKRAM05"
_b5IsDebuggingEnabled:
lda #$0
ldx #$0
.ifdef DEBUG
lda #$1
.endif
rts
.endif