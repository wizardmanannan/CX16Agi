.include "codeWindow.s"

tellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
rts