.include "codeWindow.s"
.include "picture.s"
.segment "BANKRAM05"
.import _opCounter

b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
;lda _pixelCounter
lda _logDebugVal1
rts