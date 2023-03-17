.include "global.s"
.include "codeWindow.s"
.include "debug.s"

.segment "CODE"
_initAsm:
JSRFAR tellMeTheAddressPlease, DEBUG_BANK
jsr codeWindowInit

rts