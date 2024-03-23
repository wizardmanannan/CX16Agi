.include "codeWindow.s"
.include "pictureAsm.s"
.include "irqAsm.s"
.segment "BANKRAM06"
.import _opCounter
.import _pixelCounter
_b6TellMeTheAddressPlease:
stp
lda codeWindow
lda cwCurrentCode
lda _opCounter
lda _pixelCounter
lda _logDebugVal1
lda debugVSyncCounter
lda sendIrqCommand
lda _vSyncCounter
lda _toDraw
lda _picColour
lda _bEBulkAllocatedAddresses
lda bCSplitBufferSegments
lda bCSplitBuffer
lda _bESpritesUpdatedBuffer
rts