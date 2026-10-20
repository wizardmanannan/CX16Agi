; AGI wire strings use ASCII, independently of cc65's character map.
; All scanning, decimal conversion, recursion, and bounded appending stay resident.
.macpack longbranch
.import _b5GetLogicFile, _var, _currentLog
.global _trampoline
.import _string, _b7WordText, _numInputWords, _bDObjects, _bDNumObjects
.importzp ptr1, ptr2, ptr4, tmp4, c_sp
.import incsp6, pushax
.export _expandFormatCodes, _formatIndex, _formatTextBank

.segment "BSS"
_formatSource: .res 2
_formatSourceBank: .res 1
_formatDestination: .res 2
_formatDestinationBank: .res 1
_formatRemaining: .res 2
_formatBuffered: .res 1
_formatBuffer: .res 50
_formatCode: .res 1
_formatIndex: .res 1
_formatInvalid: .res 1
_formatText: .res 2
_formatTextBank: .res 1
_formatRecursive: .res 1
formatLogicFile: .res 9
formatPreviousBank: .res 1
formatOffsetHi: .res 1
formatCursor: .res 2
formatBank: .res 1
formatDepth: .res 1
formatBasesLo: .res 8
formatBasesHi: .res 8
formatBanks: .res 8
formatCursorsLo: .res 8
formatCursorsHi: .res 8
formatWidth: .res 1
formatHasWidth: .res 1
formatDigit: .res 1
formatValue: .res 1
formatDigits: .res 3
formatCount: .res 1
formatUsed: .res 1

.segment "CODE"
; Peek at the current source. C callbacks may clobber the runtime ZP pointers.
formatPeek:
    lda formatBank
    sta $00
    lda formatCursor
    sta ptr1
    lda formatCursor+1
    sta ptr1+1
    lda (ptr1)
    rts

formatNext:
    inc formatCursor
    bne :+
    inc formatCursor+1
:   rts

formatAppend:
    ldx formatBuffered
    sta _formatBuffer,x
    inc formatBuffered
    lda _formatRemaining
    bne :+
    dec _formatRemaining+1
:   dec _formatRemaining
    lda formatBuffered
    cmp #50
    bne :+
    jsr formatFlush
:   rts

formatBuffered = _formatBuffered

; cc65 passes dstMax in AX; software-stack offsets are bank/dst/srcBank/src.
_expandFormatCodes:
    sta _formatRemaining
    stx _formatRemaining+1
    ora _formatRemaining+1
    bne :+
    jmp incsp6
:   lda $00
    sta formatPreviousBank
    ldy #0
    lda (c_sp),y
    sta _formatDestinationBank
    iny
    lda (c_sp),y
    sta _formatDestination
    iny
    lda (c_sp),y
    sta _formatDestination+1
    iny
    lda (c_sp),y
    sta _formatSourceBank
    iny
    lda (c_sp),y
    sta _formatSource
    iny
    lda (c_sp),y
    sta _formatSource+1
    lda _formatRemaining
    bne :+
    dec _formatRemaining+1
:   dec _formatRemaining
    stz _formatBuffered
    jsr formatExpand
    jsr formatFlush
    lda _formatDestinationBank
    sta $00
    lda _formatDestination
    sta ptr2
    lda _formatDestination+1
    sta ptr2+1
    lda #0
    sta (ptr2)
    lda formatPreviousBank
    sta $00
    jmp incsp6

formatFlush:
    ldy _formatBuffered
    beq @done
    lda $00
    pha
    lda _formatDestinationBank
    sta $00
    lda _formatDestination
    sta ptr2
    lda _formatDestination+1
    sta ptr2+1
    dey
@copy:
    lda _formatBuffer,y
    sta (ptr2),y
    dey
    bpl @copy
    clc
    lda _formatDestination
    adc _formatBuffered
    sta _formatDestination
    bcc :+
    inc _formatDestination+1
:   stz _formatBuffered
    pla
    sta $00
@done:
    rts

formatExpand:
    stz formatDepth
    lda _formatSource
    sta formatCursor
    sta formatBasesLo
    lda _formatSource+1
    sta formatCursor+1
    sta formatBasesHi
    lda _formatSourceBank
    sta formatBank
    sta formatBanks

formatScan:
    lda _formatRemaining
    ora _formatRemaining+1
    bne :+
    rts
:   lda formatBank
    sta $00
    lda formatCursor
    sta ptr1
    lda formatCursor+1
    sta ptr1+1
    ldx _formatBuffered
; Keep the literal run in ZP/X; flush preserves ptr1 and X.
formatLiteral:
    lda (ptr1)
    beq formatLiteralEnd
    cmp #$25
    beq formatLiteralToken
    cmp #$5c
    beq formatLiteralEscape
    sta _formatBuffer,x
    inx
    inc ptr1
    bne :+
    inc ptr1+1
:   lda _formatRemaining
    bne :+
    dec _formatRemaining+1
:   dec _formatRemaining
    cpx #50
    bne :+
    stx _formatBuffered
    jsr formatFlush
    ldx #0
:   lda _formatRemaining
    bne formatLiteral
    lda _formatRemaining+1
    bne formatLiteral
    jsr formatSaveLiteral
    rts
formatLiteralEnd:
    jsr formatSaveLiteral
    jmp formatPop
formatLiteralToken:
    jsr formatSaveLiteral
    jmp formatToken
formatLiteralEscape:
    jsr formatSaveLiteral
    jmp formatEscape
formatSaveLiteral:
    stx _formatBuffered
    lda ptr1
    sta formatCursor
    lda ptr1+1
    sta formatCursor+1
    rts

formatEscape:
    jsr formatNext
    jsr formatPeek
    bne :+
    lda #$5c
    jsr formatAppend
    jmp formatPop
:   jsr formatAppend
    jsr formatNext
    jmp formatScan

formatToken:
    jsr formatNext
    jsr formatPeek
    bne :+
    lda #$25
    jsr formatAppend
    jmp formatPop
:   sta _formatCode
    jsr formatNext
    stz _formatIndex
    stz _formatInvalid
formatParseIndex:
    jsr formatPeek
    cmp #$30
    jcc formatIndexDone
    cmp #$3a
    jcs formatIndexDone
    sec
    sbc #$30
    sta formatDigit
    lda _formatInvalid
    jne formatIndexNext
    lda _formatIndex
    cmp #25
    jcc formatIndexAccumulate
    jne formatIndexInvalid
    lda formatDigit
    cmp #6
    jcs formatIndexInvalid
formatIndexAccumulate:
    lda _formatIndex
    asl
    sta formatValue
    asl
    asl
    clc
    adc formatValue
    adc formatDigit
    sta _formatIndex
    jmp formatIndexNext
formatIndexInvalid:
    inc _formatInvalid
formatIndexNext:
    jsr formatNext
    jmp formatParseIndex

formatIndexDone:
    stz formatHasWidth
    stz formatWidth
    ldx _formatCode
    cpx #$76
    jne formatLookup
    cmp #$7c
    jne formatNumber
    inc formatHasWidth
    jsr formatNext
formatParseWidth:
    jsr formatPeek
    cmp #$30
    jcc formatNumber
    cmp #$3a
    jcs formatNumber
    sec
    sbc #$30
    sta formatDigit
    lda formatWidth
    cmp #15
    jcs formatWidthNext
    asl
    sta formatValue
    asl
    asl
    clc
    adc formatValue
    adc formatDigit
    cmp #16
    bcc :+
    lda #15
:   sta formatWidth
formatWidthNext:
    jsr formatNext
    jmp formatParseWidth

formatLookup:
    stz _formatText
    stz _formatText+1
    stz _formatRecursive
    lda _formatInvalid
    beq :+
    jmp formatScan
:   lda _formatCode
    cmp #$73
    jeq formatString
    cmp #$77
    jeq formatWord
    cmp #$30
    jeq formatObject
    cmp #$67
    beq @global
    cmp #$6d
    beq @message
    jmp formatScan
@global:
    lda #0
    bra @lookup
@message:
    inc _formatRecursive
    lda _currentLog
@lookup:
    jsr formatMessage
    sta _formatText
    stx _formatText+1
formatResolved:
    lda _formatText
    ora _formatText+1
    bne :+
    jmp formatScan
:   lda _formatRecursive
    jne formatPush
formatCopyText:
    lda _formatRemaining
    ora _formatRemaining+1
    bne :+
    rts
:   lda _formatTextBank
    sta $00
    lda _formatText
    sta ptr1
    lda _formatText+1
    sta ptr1+1
    ldx _formatBuffered
    ldy #0
; Literal replacements do not interpret percent signs or backslashes. Keep
; their cursor in ZP and buffered count in X, as for the outer literal run.
formatCopyLiteral:
    lda (ptr1),y
    beq formatCopyLiteralEnd
    sta _formatBuffer,x
    inx
    iny
    bne :+
    inc ptr1+1
:   lda _formatRemaining
    bne :+
    dec _formatRemaining+1
:   dec _formatRemaining
    cpx #50
    bne :+
    stx _formatBuffered
    phy
    jsr formatFlush
    ply
    ldx #0
:   lda _formatRemaining
    bne formatCopyLiteral
    lda _formatRemaining+1
    bne formatCopyLiteral
    stx _formatBuffered
    rts
formatCopyLiteralEnd:
    stx _formatBuffered
    jmp formatScan

; Resolve metadata through the engine's wrapped bank-5 API. Its packed
; LOGICFile offsets are checked by stringFormat.c at target compile time.
formatMessage:
    pha
    lda #<formatLogicFile
    ldx #>formatLogicFile
    jsr pushax
    ldy #5
    sty tmp4
    ldy #<_b5GetLogicFile
    sty ptr4
    ldy #>_b5GetLogicFile
    sty ptr4+1
    pla
    ldx #0
    jsr _trampoline
    lda formatLogicFile+8
    sta _formatTextBank
    lda formatLogicFile+5
    ora formatLogicFile+6
    beq @missing
    lda _formatIndex
    beq @missing
    cmp formatLogicFile+4
    beq :+
    bcs @missing
:
    dec a
    stz formatOffsetHi
    asl
    rol formatOffsetHi
    clc
    adc formatLogicFile+5
    sta ptr1
    lda formatOffsetHi
    adc formatLogicFile+6
    sta ptr1+1
    lda _formatTextBank
    sta $00
    ldy #1
    lda (ptr1),y
    tax
    lda (ptr1)
    rts
@missing:
    lda #0
    tax
    rts

formatTimes5:
    stz formatOffsetHi
    sta formatValue
    asl
    rol formatOffsetHi
    asl
    rol formatOffsetHi
    clc
    adc formatValue
    bcc :+
    inc formatOffsetHi
:   rts

formatString:
    lda _formatIndex
    cmp #12
    jcs formatScan
    jsr formatTimes5
    .repeat 3
    asl
    rol formatOffsetHi
    .endrepeat
    clc
    adc #<_string
    sta _formatText
    lda formatOffsetHi
    adc #>_string
    sta _formatText+1
    lda #7
    sta _formatTextBank
    inc _formatRecursive
    jmp formatResolved

formatWord:
    lda _formatIndex
    jeq formatScan
    cmp #11
    jcs formatScan
    ldx _numInputWords+1
    jmi formatScan
    bne @valid
    cmp _numInputWords
    beq @valid
    jcs formatScan
@valid:
    dec
    jsr formatTimes5
    .repeat 4
    asl
    rol formatOffsetHi
    .endrepeat
    clc
    adc #<_b7WordText
    sta _formatText
    lda formatOffsetHi
    adc #>_b7WordText
    sta _formatText+1
    lda #7
    sta _formatTextBank
    jmp formatResolved

formatObject:
    lda _formatIndex
    jeq formatScan
    cmp #101
    jcs formatScan
    lda #13
    sta $00
    ldx _bDNumObjects+1
    bmi @invalid
    lda _formatIndex
    cpx #0
    bne @valid
    cmp _bDNumObjects
    beq @valid
    bcs @invalid
@valid:
    dec
    jsr formatTimes5
    clc
    adc #<(_bDObjects+1)
    sta ptr1
    lda formatOffsetHi
    adc #>(_bDObjects+1)
    sta ptr1+1
    lda (ptr1)
    sta _formatText
    ldy #1
    lda (ptr1),y
    sta _formatText+1
    lda #13
    sta _formatTextBank
@invalid:
    lda formatBank
    sta $00
    jmp formatResolved

formatPush:
    ldx formatDepth
    cpx #7
    bcc :+
    jmp formatScan
:   ldx #0
formatCheckCycle:
    lda formatBasesLo,x
    cmp _formatText
    jne formatCycleNext
    lda formatBasesHi,x
    cmp _formatText+1
    jne formatCycleNext
    lda formatBanks,x
    cmp _formatTextBank
    jne formatCycleNext
    jmp formatScan
formatCycleNext:
    cpx formatDepth
    jeq formatSaveFrame
    inx
    jmp formatCheckCycle
formatSaveFrame:
    lda formatCursor
    sta formatCursorsLo,x
    lda formatCursor+1
    sta formatCursorsHi,x
    inx
    stx formatDepth
    lda _formatText
    sta formatBasesLo,x
    sta formatCursor
    lda _formatText+1
    sta formatBasesHi,x
    sta formatCursor+1
    lda _formatTextBank
    sta formatBanks,x
    sta formatBank
    jmp formatScan

formatPop:
    ldx formatDepth
    bne :+
    rts
:   dex
    stx formatDepth
    lda formatCursorsLo,x
    sta formatCursor
    lda formatCursorsHi,x
    sta formatCursor+1
    lda formatBanks,x
    sta formatBank
    jmp formatScan

formatNumber:
    lda _formatInvalid
    beq :+
    jmp formatScan
:   lda _var
    sta ptr1
    lda _var+1
    sta ptr1+1
    ldy _formatIndex
    lda (ptr1),y
    ldx #0
formatHundreds:
    cmp #100
    bcc :+
    sbc #100
    inx
    jmp formatHundreds
:   stx formatDigits+2
    ldx #0
formatTens:
    cmp #10
    bcc :+
    sbc #10
    inx
    jmp formatTens
:   sta formatDigits
    stx formatDigits+1
    ldx #3
    lda formatDigits+2
    jne formatHaveCount
    dex
    lda formatDigits+1
    jne formatHaveCount
    dex
formatHaveCount:
    stx formatCount
    lda formatHasWidth
    bne :+
    stx formatWidth
:   lda formatWidth
    cmp formatCount
    bcc :+
    lda formatCount
:   sta formatUsed
formatPadding:
    lda formatWidth
    cmp formatUsed
    jeq formatEmitDigits
    lda _formatRemaining
    ora _formatRemaining+1
    bne :+
    rts
:   lda #$30
    jsr formatAppend
    dec formatWidth
    jmp formatPadding
formatEmitDigits:
    lda formatUsed
    jeq formatNumberDone
    lda _formatRemaining
    ora _formatRemaining+1
    bne :+
    rts
:   dec formatUsed
    ldx formatUsed
    lda formatDigits,x
    clc
    adc #$30
    jsr formatAppend
    jmp formatEmitDigits
formatNumberDone:
    jmp formatScan
