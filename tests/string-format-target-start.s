.setcpu "W65C02"

.importzp c_sp
.importzp tmp4, ptr4
.import callptr4
.import _targetPrepareKq3
.import _targetExpandKq3
.import _targetValidateKq3
.import _targetBridge
.import _targetEdgeCases

.export targetPrepareKq3Entry
.export targetExpandKq3Entry
.export targetValidateKq3Entry
.export targetBridgeEntry
.export targetEdgeCasesEntry
.import _targetPrepareLiteral
.export targetPrepareLiteralEntry
.import _targetExpandLiteral
.export targetExpandLiteralEntry
.import _targetValidateLiteral
.export targetValidateLiteralEntry
.export _trampoline

.segment "CODE"

initCStack:
    lda #<$8F00
    sta c_sp
    lda #>$8F00
    sta c_sp+1
    rts

; The logic lookup declaration uses the application's bank-5 wrapped-call ABI.
_trampoline:
    sta trampolineA
    lda $00
    pha
    lda tmp4
    sta $00
    lda trampolineA
    jsr callptr4
    sta trampolineA
    pla
    sta $00
    lda trampolineA
    rts
trampolineA:
    .byte 0

targetPrepareKq3Entry:
    jsr initCStack
    jsr _targetPrepareKq3
    rts

targetExpandKq3Entry:
    jsr initCStack
    jsr _targetExpandKq3
    rts

targetValidateKq3Entry:
    jsr initCStack
    jsr _targetValidateKq3
    rts

targetEdgeCasesEntry:
    jsr initCStack
    jsr _targetEdgeCases
    rts

targetBridgeEntry:
    jsr initCStack
    jsr _targetBridge
    rts

targetPrepareLiteralEntry:
    jsr initCStack
    jsr _targetPrepareLiteral
    rts

targetExpandLiteralEntry:
    jsr initCStack
    jsr _targetExpandLiteral
    rts

targetValidateLiteralEntry:
    jsr initCStack
    jsr _targetValidateLiteral
    rts
