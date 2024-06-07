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