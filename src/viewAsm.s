
.ifndef VIEW_INC
; Set the value of include guard and define constants
VIEW_INC = 1

.include "globalViews.s"

.segment "BANKRAM09"
_viewHeaderBuffer: .res VIEW_HEADER_BUFFER_SIZE
_loopHeaderBuffer: .res LOOP_HEADER_BUFFER_SIZE

.endif


