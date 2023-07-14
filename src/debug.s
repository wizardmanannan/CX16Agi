.include "codeWindow.s"
.include "picture.s"
.segment "BANKRAM05"
.import _opCounter

b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
lda bresenham_x1
lda bresenham_x2
lda bresenham_y1
lda bresenham_y2
rts