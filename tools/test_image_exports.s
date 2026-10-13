; Export resident internals for the full production IRQ execution test.
; The production instructions remain those in main.s.
.include "main.s"
.export cwCurrentCode
.export custom_irq_handler
.export handleScreenShake
.export shakeCount
.export shakeFrames
.export shakePhases
.export shakeWindow
.export shakeBorder
.export b4ShakeScreen
