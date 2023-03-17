.include "codeWindow.s"
.segment "BANKRAM05"
.import _opCounter

tellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
rts