.include "codeWindow.s"
.include "picture.s"
.segment "BANKRAM05"
.import _opCounter

b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
lda _bresenham_x1
lda _bresenham_x2
lda _bresenham_y1
lda _bresenham_y2
rts