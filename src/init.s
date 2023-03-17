.include "global.s"
.include "codeWindow.s"
.include "debug.s"

.segment "BANKRAM07"
_initAsm:
JSRFAR tellMeTheAddressPlease, DEBUG_BANK
jsr codeWindowInit

rts
.segment "CODE"