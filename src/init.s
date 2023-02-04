.include "global.s"
.include "codeWindow.s"
.include "debug.s"

_initAsm:
jsr tellMeTheAddressPlease
jsr codeWindowInit
rts