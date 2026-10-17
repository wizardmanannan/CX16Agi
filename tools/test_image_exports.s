; Export resident internals for the full production IRQ execution test.
; The production instructions remain those in main.s.
.include "main.s"
.export default_irq_vector
.export cwCurrentCode
.export custom_irq_handler
.export handleScreenShake
.export shakeCount
.export shakeFrames
.export shakePhases
.export shakeWindow
.export shakeBorder
.export b4ShakeScreen
.export _testMultiply8x8

.segment "CODE"
_testMultiply8x8:
jsr _b6EnterForegroundVera
tay
txa
MULT_8x8_16
jsr _b6LeaveForegroundVera
rts
