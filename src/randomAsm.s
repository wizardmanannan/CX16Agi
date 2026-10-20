.ifndef RANDOM_INC
RANDOM_INC = 1
.segment "CODE"

.import _b6RandomNumbers
.ifdef LOGIC_APP_REPLAY_TARGET
.import _logicAppReplayRandomNumber
.endif

; byte rand8Bit(byte exclusiveBound); zero selects all 256 byte values.
_rand8Bit:
.ifdef LOGIC_APP_REPLAY_TARGET
; rand8Bit's bound is exclusive; ScummVM RandomSource's is inclusive.
dec
jmp _logicAppReplayRandomNumber
.else
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
.endif
@randomCounter: .byte $0

; A=min, X=max (max >= min); returns A in min..max inclusive.
; X, Y and sreg are scratch. A zero exclusive width selects all 256 values.
; Replay uses the reference generator's inclusive bound directly.

_randBetween:
pha
jsr popa
plx

;a min x max

randBetweenAsmCall:
        sta sreg        ; sreg = min
        pha             ; save min across the call

sec
txa
sbc sreg
.ifdef LOGIC_APP_REPLAY_TARGET
; AGI random(min,max) includes max, unlike the rand8Bit helper contract.
jsr _logicAppReplayRandomNumber
.else
; Inclusive width; 256 wraps to the rand8Bit/mod8 full-byte sentinel zero.
inc
jsr _rand8Bit
.endif

plx
stx sreg

clc
adc sreg
rts
.endif
