.include "codeWindow.s"
.include "pictureAsm.s"
.segment "BANKRAM05"
.import _opCounter
.import _pixelCounter
b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
lda _pixelCounter
lda _logDebugVal1
lda _okFillAddress
lda debugVSyncCounter
rts