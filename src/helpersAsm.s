.ifndef  HELPERS_INC

HELPERS_INC = 1

.export _trampoline
.import popa
.importzp tmp4
.import callptr4
.import ptr4

_trampoline:
sta @aVal

lda $0
pha

lda tmp4
sta $0

lda @aVal
jsr callptr4

pla
sta $0
rts
@aVal: .byte $0

.endif