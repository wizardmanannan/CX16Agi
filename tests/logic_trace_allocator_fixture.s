.segment "CODE"

.importzp tmp4, ptr4
.import callptr4

.global _trampoline
_trampoline:
    sta @trampoline_a
    lda $00
    pha
    lda tmp4
    sta $00
    lda @trampoline_a
    jsr callptr4
    sta @trampoline_a
    pla
    sta $00
    lda @trampoline_a
    rts
@trampoline_a:
    .byte $00

.global _logicContextCaptureCaller
_logicContextCaptureCaller:
    rts

.global _logicContextRestoreCaller
_logicContextRestoreCaller:
    rts

.global _logicContextCaptureStackPointers
_logicContextCaptureStackPointers:
    rts

; The general CX16 runner validates these labels even when it only runs the
; allocator-specific result case.
.global b2CallAfterLogic
b2CallAfterLogic:
    rts

.global b2CallVAfterLogic
b2CallVAfterLogic:
    rts
