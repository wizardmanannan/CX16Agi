.ifndef DEBUG_INC
DEBUG_INC = 1
.include "codeWindow.s"
.include "pictureAsm.s"
.include "irqAsm.s"
.segment "BANKRAM06"
.import _opCounter
.import _pixelCounter
_b6TellMeTheAddressPlease:
;stp
lda _opCounter
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