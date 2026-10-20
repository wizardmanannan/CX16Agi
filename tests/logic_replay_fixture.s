.importzp sreg
.import pusha
.import _replay_checkpoint_length, _replay_checkpoint
.import _replay_rng_minimum, _replay_rng_maximum, _replay_rng_next

LOGIC_DISPATCH_TEST_HOOK = 1
.macro TRAMPOLINE BANK, JMPTO
    jsr JMPTO
.endmacro

.include "commandLoop.s"

.segment "CODE"
.global _replay_dispatch_logic_zero
.global _replay_execute
.import _replay_execute_body

; RUN enters at this exported wrapper. The explicit JSR gives the C replay
; body its normal cc65 hardware-stack call frame before the testbench stop.
_replay_execute:
    jsr _replay_execute_body
    jmp $FFFD

; Set a deterministic synthetic caller frame (or restore the LCX1 direct-logic
; fixture's declared frame), then enter the unchanged production dispatch.
_replay_dispatch_logic_zero:
    lda #<$A300
    sta _startPos
    lda #>$A300
    sta _startPos + 1
    lda #<$A380
    sta _endPos
    lda #>$A380
    sta _endPos + 1
    lda #<$A300
    sta ZP_PTR_CODE
    lda #>$A300
    sta ZP_PTR_CODE + 1
    stz cwCurrentCode
    stz cwCurrentCode + 1
    lda #$12
    sta ZP_PTR_LE
    lda #$34
    sta ZP_PTR_LE + 1
    lda #$78
    sta ZP_PTR_LF
    lda #$56
    sta ZP_PTR_LF + 1
    lda #43
    sta _codeBank
    lda #$39
    sta RAM_BANK
    stz codeWindowInvalid

    lda _replay_checkpoint_length
    cmp #20
    bcc @dispatch
    lda _replay_checkpoint
    cmp #$4C
    bne @dispatch
    lda _replay_checkpoint + 1
    cmp #$43
    bne @dispatch
    lda _replay_checkpoint + 2
    cmp #$58
    bne @dispatch
    lda _replay_checkpoint + 3
    cmp #$31
    bne @dispatch

    lda _replay_checkpoint + 4
    sta _startPos
    lda _replay_checkpoint + 5
    sta _startPos + 1
    lda _replay_checkpoint + 6
    sta _endPos
    lda _replay_checkpoint + 7
    sta _endPos + 1
    lda _replay_checkpoint + 8
    sta ZP_PTR_CODE
    lda _replay_checkpoint + 9
    sta ZP_PTR_CODE + 1
    lda _replay_checkpoint + 10
    sta ZP_PTR_LE
    lda _replay_checkpoint + 11
    sta ZP_PTR_LE + 1
    lda _replay_checkpoint + 12
    sta ZP_PTR_LF
    lda _replay_checkpoint + 13
    sta ZP_PTR_LF + 1
    lda _replay_checkpoint + 14
    sta _codeBank
    lda _replay_checkpoint + 15
    sta RAM_BANK
    lda _replay_checkpoint + 16
    sta cwCurrentCode
    lda _replay_checkpoint + 17
    sta codeWindowInvalid

@dispatch:
    lda #0
    jsr callLogic
    rts

; b4Random_num uses this production platform seam with A=min, X=max. The
; opcode handler itself remains the interpreter's normal implementation.
randBetweenAsmCall:
    sta _replay_rng_minimum
    stx _replay_rng_maximum
    jsr _replay_rng_next
    rts

b1IsControllerSet:
    lda #0
    rts
