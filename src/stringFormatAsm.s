; AGI wire strings use ASCII, independently of cc65's character map.
; All scanning, decimal conversion, recursion, and bounded appending stay resident.
.macpack longbranch
.import _formatSource, _formatSourceBank, _formatRemaining
.import _formatBuffered, _formatBuffer, _formatFlush, _formatResolve
.import _formatCode, _formatIndex, _formatInvalid
.import _formatText, _formatTextBank, _formatRecursive, _var
.importzp ptr1
.export _formatExpandAsm

.segment "BSS"
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
    jsr _formatFlush
:   rts

; Keep imports spelled like the C workspace names above.
formatBuffered = _formatBuffered

_formatExpandAsm:
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
:   jsr formatPeek
    bne :+
    jmp formatPop
:   cmp #$25
    jeq formatToken
    cmp #$5c
    jeq formatEscape
    jsr formatAppend
    jsr formatNext
    jmp formatScan

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
    jsr _formatResolve
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
    lda (ptr1)
    bne :+
    jmp formatScan
:   jsr formatAppend
    inc _formatText
    jne formatCopyText
    inc _formatText+1
    jmp formatCopyText

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
