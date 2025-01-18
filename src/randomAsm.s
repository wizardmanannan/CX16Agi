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

;byte randBetween(byte min, byte max)
_randBetween:
pha
jsr popa
plx

;a min x max
randBetweenAsmCall:
sta sreg
pha

sec
txa
sbc sreg
jsr _rand8Bit

plx
stx sreg

clc
adc sreg
rts
.endif