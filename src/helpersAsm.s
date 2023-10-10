.ifndef  HELPERS_INC

HELPERS_INC = 1

.export _trampoline
.import popa
.importzp tmp4
.import callptr4
.import ptr4

_trampoline:
pha

lda $0
sta @previousRamBank

lda tmp4
sta $0

pla
jsr callptr4

lda @previousRamBank
sta $0
rts
@previousRamBank: .byte $0

.endif