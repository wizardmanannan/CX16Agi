.include "global.s"
.include "codeWindow.s"
.include "debug.s"

.segment "CODE"
_initAsm:
jsr tellMeTheAddressPlease
jsr codeWindowInit

rts