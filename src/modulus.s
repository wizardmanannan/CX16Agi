.ifndef MOD_INC
MOD_INC = 1

; zero-page addresses (adjust as needed)
MOD_DIVIDEND  = sreg
MOD_DIVISOR   = sreg + 1
MOD_REMAINDER = sreg2

; ------------------------------------------------------------
; mod8:
;   8-bit modulus routine: MOD_REMAINDER = MOD_DIVIDEND % MOD_DIVISOR
;   destroys: a, x
; ------------------------------------------------------------
mod8:
        ; clear MOD_REMAINDER
        lda #$00
        sta MOD_REMAINDER

        ; do 8 iterations (one for each bit in the MOD_DIVIDEND)
        ldx #$08

loop_mod8:
        ; shift MOD_DIVIDEND left by 1 (carry gets set if high bit was 1)
        asl MOD_DIVIDEND

        ; rotate MOD_REMAINDER left by 1, pulling in carry from 'asl MOD_DIVIDEND'
        rol MOD_REMAINDER

        ; compare MOD_REMAINDER to MOD_DIVISOR
        lda MOD_REMAINDER
        cmp MOD_DIVISOR
        bcc no_sub      ; if MOD_REMAINDER < MOD_DIVISOR, skip subtraction

        ; MOD_REMAINDER = MOD_REMAINDER - MOD_DIVISOR
        sec
        sbc MOD_DIVISOR
        sta MOD_REMAINDER

no_sub:
        dex
        bne loop_mod8

        rts             ; on return, MOD_REMAINDER holds the 8-bit modulus

.endif