.include "global.s"
.include "codeWindow.s"
.include "debug.s"

; Import required C variables
.import _logicEntryAddressesLow
.import _logicEntryAddressesHigh

.segment "BANKRAM07"
_b7InitAsm:
    JSRFAR b5TellMeTheAddressPlease, DEBUG_BANK
    jsr b7CodeWindowInit
    
    lda _logicEntryAddressesLow
    sta ZP_PTR_PLF_LOW
    lda _logicEntryAddressesLow + 1
    sta ZP_PTR_PLF_LOW + 1

    lda _logicEntryAddressesHigh
    sta ZP_PTR_PLF_HIGH
    lda _logicEntryAddressesHigh + 1
    sta ZP_PTR_PLF_HIGH + 1
rts
.segment "CODE" ;Not sure why this is needed TODO:Fix