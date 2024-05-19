.ifndef  INPUTIRQ_INC
GLOBAL_INPUTIRQ_INC = 1
.import _inputLineDisplayed

.include "globalGraphics.s"
.segment "BANKRAM07"
b7HandleInputLine:
lda _inputLineDisplayed
@drawInput:
SET_VERA_ADDRESS_IMMEDIATE INPUT_STRING_ADDRESS, #$0, #$1
lda #INPUT_PROMPT_CHAR
sta VERA_data0

bra @end

@end:
rts
.endif


