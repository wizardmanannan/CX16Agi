.ifndef  HELPERS_INC

HELPERS_INC = 1

.segment "CODE"

.export _trampoline
.import popa
.importzp tmp4
.import callptr4
.import ptr4

_trampoline:
sta @aVal ;Preserve a argument

lda $0
pha

lda tmp4
sta $0

lda @aVal
jsr callptr4
sta @aVal ;Perserve return a

pla
sta $0

lda @aVal
rts
@aVal: .byte $0

.endif