.include "codeWindow.s"
.segment "BANKRAM05"
.import _opCounter

b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
rts