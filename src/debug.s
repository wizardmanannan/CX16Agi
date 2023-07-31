.include "codeWindow.s"
.include "picture.s"
.segment "BANKRAM05"
.import _opCounter
.import _pixelCounter

b5TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
lda _pixelCounter
lda bresenham_x1
lda bresenham_x2
lda bresenham_y1
lda bresenham_y2
rts