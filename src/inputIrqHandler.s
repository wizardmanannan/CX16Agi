.ifndef  INPUTIRQ_INC
GLOBAL_INPUTIRQ_INC = 1
.import _inputLineDisplayed

.include "globalGraphics.s"

INPUT_LINE_NUMBER = 20
INPUT_LINE_COL = 3
MAX_BUFFER_SIZE = 80 
INPUT_STRING_ADDRESS = MAP_BASE + (FIRST_ROW + INPUT_LINE_NUMBER - 1) * TILE_LAYER_BYTES_PER_ROW + INPUT_LINE_COL * BYTES_PER_CELL;
.segment "BANKRAM07"
b7HandleInputLine:
lda _inputLineDisplayed
@drawInput:
SET_VERA_ADDRESS_IMMEDIATE INPUT_STRING_ADDRESS, #$0, #$2
;lda #$61
;sta VERA_data0

bra @end

@end:
rts
.endif


