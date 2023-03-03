.include "codeWindow.s"

.import _opCounter

tellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
rts