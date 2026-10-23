; L0 is a bitmap and sprites have no layer scroll. Move the composed display
; instead: +8/+4 AGI pixels at DISPLAY_SCALE=64, without copying VRAM.
; Eight alternating phases per repeat, four VSYNCs per phase (AGI timing).
.assert DISPLAY_SCALE_GRAPHICS = 64, error, "Shake displacement requires 2x display scaling"
.segment "ZEROPAGE"
shakeFrames: .res 1 ; written before first read; linker-owned, not IRQ scratch
.assert shakeFrames < $7E, lderror, "Shake frame counter overlaps KERNAL scratch"
.segment "CODE"
shakePhases: .byte 0
shakeWindow: .res 4
shakeShiftedWindow: .res 4
shakeBorder: .byte 0

; Called only by the VSYNC count gate, with a nonzero shakeCount.
handleScreenShake:
lda shakePhases
beq @prepare
dec shakeFrames
beq @prepare
rts ; held frames do not touch VERA
@prepare:
lda VERA_ctrl
pha
and #1
sta VERA_ctrl ; DCSEL=0, retain ADDRSEL
lda shakePhases
bne @tick

lda VERA_dc_border
sta shakeBorder
stz VERA_dc_border ; exposed edges must be black
lda VERA_ctrl
ora #2
sta VERA_ctrl ; DCSEL=1
; Fixed four-byte transfers: ca65 unrolls these, avoiding indexed/loop costs.
.repeat 4, offset
lda VERA_dc_hstart+3-offset
sta shakeWindow+3-offset
sta shakeShiftedWindow+3-offset
.endrepeat

; Keep each axis unchanged if shifting its stop would wrap a byte.
.repeat 2, axis
lda shakeWindow+3-axis*2
clc
adc #4
bcs :+
sta shakeShiftedWindow+3-axis*2
lda shakeWindow+2-axis*2
; Successful stop addition cleared C; STA/LDA preserve it.
adc #4
sta shakeShiftedWindow+2-axis*2
:
.endrepeat
lda #8
sta shakePhases
bra @shifted

@tick:
dec shakePhases
bne @phase
dec shakeCount
beq @finish
lda #8
sta shakePhases
@phase:
lda VERA_ctrl
ora #2
sta VERA_ctrl
lda shakePhases
lsr ; phase parity in carry, no mask byte needed
bcs @original
@shifted:
.repeat 4, offset
lda shakeShiftedWindow+3-offset
sta VERA_dc_hstart+3-offset
.endrepeat
bra @hold
@original:
.repeat 4, offset
lda shakeWindow+3-offset
sta VERA_dc_hstart+3-offset
.endrepeat
@hold:
lda #4
sta shakeFrames
@end:
pla
sta VERA_ctrl
rts
@finish:
; The last phase already restored the window. Restore the border too.
lda shakeBorder
sta VERA_dc_border
bra @end

.segment "BANKRAM04"
b4ShakeScreen:
jsr _loadAndIncWinCode
sta shakeCount
tax ; set operand flags; dispatch overwrites X before using it
beq @return
; Interpreter stays blocked, but sound, input and VSYNC IRQs keep running.
@wait:
sei
lda shakeCount
beq @done
; WAI wakes even with IRQ masked; CLI then services the pending IRQ.
; This avoids missing completion between checking the count and sleeping.
wai
cli
bra @wait
@done:
cli
@return:
jmp mainLoop
