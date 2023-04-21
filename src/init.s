.include "global.s"
.include "codeWindow.s"
.include "debug.s"

.segment "BANKRAM07"
_b7InitAsm:
JSRFAR b5TellMeTheAddressPlease, DEBUG_BANK
jsr b7CodeWindowInit

rts
.segment "CODE" ;Not sure why this is needed TODO:Fix