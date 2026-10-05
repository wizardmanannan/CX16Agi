.ifndef RANDOM_INC
RANDOM_INC = 1
.segment "CODE"

.import _b6RandomNumbers

;byte rand8Bit(byte max);
_rand8Bit:
sta MOD_DIVISOR

lda RAM_BANK
pha

lda #RANDOM_BANK
sta RAM_BANK

ldx @randomCounter
lda _b6RandomNumbers,x
sta MOD_DIVIDEND
inx
stx @randomCounter

jsr mod8
lda MOD_REMAINDER

plx
stx RAM_BANK

rts
@randomCounter: .byte $0

; randBetweenAsmCall
; In:  A = min, X = max (unsigned 8-bit, max >= min)
; Out: A = value in min..max inclusive
;      X clobbered, Y preserved, carry clear
; sreg is scratch and is not preserved.
;
; _rand8Bit(n) returns 0..n-1, so you must add one to max
; before subtracting min. Passing max - min alone never
; returns max.
;
; Do not call with max < min (the subtract borrows and the span wraps).
; Adding one to max wraps if max is 255. That makes the span 0
; and _rand8Bit(0) is not a valid modulus, so 0..255 is a special case.

_randBetween:
pha
jsr popa
plx

;a min x max

randBetweenAsmCall:
        sta sreg        ; sreg = min
        pha             ; save min across the call

        inx        ; add one to max: exclusive upper bound
        sec
        txa
        sbc sreg        ; A = (max + 1) - min
        jsr _rand8Bit   ; A = 0 .. max-min inclusive

        plx             ; X = saved min
        stx sreg
        clc
        adc sreg        ; A = random + min
        rts
.endif