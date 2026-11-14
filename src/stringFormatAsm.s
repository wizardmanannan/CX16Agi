; =====================================================================
; stringFormatAsm.s
;
; Expands AGI-style format strings (the classic "%v12", "%s3", "%w1",
; "%o5", "%m2", "%g7", "%v5|4" etc.) into a destination buffer.
;
; AGI wire strings are plain ASCII, independent of cc65's character map.
; All scanning, decimal conversion, recursion and bounded appending stay
; resident (no bank switching inside the hot loops except when we must
; read/write the actual string data).
;
; Call convention (cc65):
;   AX  = dstMax (maximum bytes we are allowed to write)
;   software stack (top to bottom):
;     dstBank, dst, srcBank, src
;
; Returns nothing; the destination is null-terminated on success.
; If dstMax == 0 the routine just cleans the stack and returns.
;
; Scalar state lives in the supplied zero-page slots.
; The 50-byte output buffer, the LOGICFile temporary and the
; recursion-frame arrays remain in BSS.
; =====================================================================

.macpack longbranch

.import _b5GetLogicFile, _var, _currentLog
.global _trampoline
.import _string, _b7WordText, _numInputWords, _bDObjects, _bDNumObjects
.importzp ptr1, ptr2, ptr4, tmp4, sp
.import incsp6, pushax
.export _expandFormatCodes, _formatIndex, _formatTextBank

; ---------------------------------------------------------------------
; Zero-page assignments (each ZP_TMP_* is a 2-byte slot)
; ---------------------------------------------------------------------

; 2-byte pointers / counters
_formatSource           = ZP_TMP_2      ; FORMAT_SOURCE
_formatDestination      = ZP_TMP_3
_formatRemaining        = ZP_TMP_4
_formatText             = ZP_TMP_5
formatCursor            = ZP_TMP_6

; 1-byte flags / small integers packed two-per-slot
_formatSourceBank       = ZP_TMP_7
_formatDestinationBank  = ZP_TMP_7+1
_formatCode             = ZP_TMP_8
_formatIndex            = ZP_TMP_8+1    ; exported
_formatInvalid          = ZP_TMP_9
_formatTextBank         = ZP_TMP_9+1    ; exported
_formatRecursive        = ZP_TMP_10
formatRunStart          = ZP_TMP_10+1
formatRunLimit          = ZP_TMP_12
formatPreviousBank      = ZP_TMP_12+1
formatOffsetHi          = ZP_TMP_13
formatBank              = ZP_TMP_13+1
formatDepth             = ZP_TMP_14
formatWidth             = ZP_TMP_14+1
formatHasWidth          = ZP_TMP_16
formatDigit             = ZP_TMP_16+1
formatValue             = ZP_TMP_17
formatCount             = ZP_TMP_17+1
formatUsed              = ZP_TMP_18
; ZP_TMP_18+1 and ZP_TMP_19–ZP_TMP_27 are free

; ---------------------------------------------------------------------
; BSS – only the items that will not fit in the supplied ZP
; ---------------------------------------------------------------------
.segment "BSS"

_formatBuffered:        .res 1
_formatBuffer:          .res 50         ; kept in BSS (too large for ZP)
formatLogicFile:        .res 9          ; temporary for the bank-5 call
formatBasesLo:          .res 8
formatBasesHi:          .res 8
formatBanks:            .res 8
formatCursorsLo:        .res 8
formatCursorsHi:        .res 8
formatDigits:           .res 3

; ---------------------------------------------------------------------
; CODE
; ---------------------------------------------------------------------
.segment "CODE"

; ---------------------------------------------------------------------
; formatPeek
; Returns the byte at the current cursor (in A).
; Bank is set from formatBank; pointer is loaded into ptr1.
; Does not advance the cursor.
; ---------------------------------------------------------------------
formatPeek:
        lda formatBank
        sta $00                 ; X16 bank register
        lda formatCursor
        sta ptr1
        lda formatCursor+1
        sta ptr1+1
        lda (ptr1)
        rts

; ---------------------------------------------------------------------
; formatNext
; Advances the 16-bit cursor by one.
; ---------------------------------------------------------------------
formatNext:
        inc formatCursor
        bne :+
        inc formatCursor+1
:       rts

; ---------------------------------------------------------------------
; formatAppend
; Appends the byte in A to the local buffer.
; When the buffer reaches 50 bytes it is flushed.
; Also decrements the remaining-byte counter.
; ---------------------------------------------------------------------
formatAppend:
        ldx _formatBuffered
        sta _formatBuffer,x
        inc _formatBuffered
        lda _formatRemaining
        bne :+
        dec _formatRemaining+1
:       dec _formatRemaining
        lda _formatBuffered
        cmp #50
        bne :+
        jsr formatFlush
:       rts

; Alias so the rest of the code can use the shorter name
formatBuffered = _formatBuffered

; =====================================================================
; _expandFormatCodes  (exported entry point)
;
; AX = dstMax
; Stack: dstBank, dst, srcBank, src
; =====================================================================
.export _expandFormatCodes
_expandFormatCodes:
        sta _formatRemaining
        stx _formatRemaining+1
        ora _formatRemaining+1
        bne :+
        jmp incsp6              ; dstMax == 0 → just clean stack and leave

:       lda $00
        sta formatPreviousBank  ; remember caller's bank

        ; Pull the four parameters off the software stack
        ldy #0
        lda (sp),y
        sta _formatDestinationBank
        iny
        lda (sp),y
        sta _formatDestination
        iny
        lda (sp),y
        sta _formatDestination+1
        iny
        lda (sp),y
        sta _formatSourceBank
        iny
        lda (sp),y
        sta _formatSource
        iny
        lda (sp),y
        sta _formatSource+1

        ; We will write one byte less than dstMax so we always have room
        ; for the terminating NUL.
        lda _formatRemaining
        bne :+
        dec _formatRemaining+1
:       dec _formatRemaining

        stz _formatBuffered
        jsr formatExpand        ; do the real work
        jsr formatFlush         ; flush any leftover buffered bytes

        ; Null-terminate the destination
        lda _formatDestinationBank
        sta $00
        lda _formatDestination
        sta ptr2
        lda _formatDestination+1
        sta ptr2+1
        lda #0
        sta (ptr2)

        ; Restore the caller's bank and clean the stack
        lda formatPreviousBank
        sta $00
        jmp incsp6

; ---------------------------------------------------------------------
; formatFlush
; Copies the local buffer into the real destination (which may be in
; another bank) and advances the destination pointer.
; ---------------------------------------------------------------------
formatFlush:
        ldy _formatBuffered
        beq @done
        lda $00
        pha                     ; save current bank
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
:       stz _formatBuffered
        pla
        sta $00                 ; restore bank
@done:
        rts

; ---------------------------------------------------------------------
; formatExpand
; Top-level driver. Initialises the recursion frames and starts scanning.
; ---------------------------------------------------------------------
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

; ---------------------------------------------------------------------
; formatScan
; Main loop: keep reading until we hit a NUL, a '%', a '\', or we run
; out of destination space.
; ---------------------------------------------------------------------
formatScan:
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts                     ; no more room

:       lda formatBank
        sta $00
        lda formatCursor
        sta ptr1
        lda formatCursor+1
        sta ptr1+1
        ldx _formatBuffered
        ldy #0
        jsr formatBeginRun      ; set up how many literal bytes we may copy

; Bound the whole run before scanning; account for bytes at a token/flush/end.
formatLiteral:
        lda (ptr1),y
        beq formatLiteralEnd
        cmp #$25                ; '%'
        beq formatLiteralToken
        cmp #$5c                ; '\'
        beq formatLiteralEscape
        sta _formatBuffer,x
        inx
        iny
        bne :+
        inc ptr1+1
:       cpx formatRunLimit
        bne formatLiteral
        jsr formatEndRun
        cpx #50
        bne :+
        phy
        jsr formatFlush
        ply
        ldx #0
:       lda _formatRemaining
        ora _formatRemaining+1
        beq :+
        jsr formatBeginRun
        bra formatLiteral
:       jsr formatSaveCursor
        rts

formatLiteralEnd:
        jsr formatSaveLiteral
        jmp formatPop           ; end of this string → pop recursion frame

formatLiteralToken:
        jsr formatSaveLiteral
        jmp formatToken

formatLiteralEscape:
        jsr formatSaveLiteral
        jmp formatEscape

formatSaveLiteral:
        jsr formatEndRun
formatSaveCursor:
        tya
        clc
        adc ptr1
        sta formatCursor
        lda ptr1+1
        adc #0
        sta formatCursor+1
        rts

; ---------------------------------------------------------------------
; formatBeginRun / formatEndRun
;
; These two helpers let us copy a run of literal characters with a
; simple indexed loop while still honouring the remaining-byte limit
; and the 50-byte local buffer size.
; Neither routine disturbs X, Y or the source pointer.
; ---------------------------------------------------------------------
formatBeginRun:
        stx formatRunStart
        lda #50
        sta formatRunLimit
        lda _formatRemaining+1
        bne @done
        txa
        clc
        adc _formatRemaining
        bcs @done
        cmp #50
        bcs @done
        sta formatRunLimit
@done:
        rts

formatEndRun:
        stx _formatBuffered
        txa
        sec
        sbc formatRunStart
        sta formatRunStart
        sec
        lda _formatRemaining
        sbc formatRunStart
        sta _formatRemaining
        bcs @done
        dec _formatRemaining+1
@done:
        rts

; ---------------------------------------------------------------------
; formatEscape
; Handles a backslash. The next character is emitted literally
; (or a lone '\' if we are at the end of the string).
; ---------------------------------------------------------------------
formatEscape:
        jsr formatNext
        jsr formatPeek
        bne :+
        lda #$5c
        jsr formatAppend
        jmp formatPop
:       jsr formatAppend
        jsr formatNext
        jmp formatScan

; ---------------------------------------------------------------------
; formatToken
; We have just seen a '%'. Parse the optional index and then dispatch
; on the format letter.
; ---------------------------------------------------------------------
formatToken:
        jsr formatNext
        jsr formatPeek
        bne :+
        lda #$25                ; lone '%' at end of string
        jsr formatAppend
        jmp formatPop
:       sta _formatCode
        jsr formatNext
        stz _formatIndex
        stz _formatInvalid

; Parse a decimal index (0-255) after the format letter.
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
        ; index = index * 10 + digit
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
        cpx #$76                ; 'v' ?
        jne formatLookup
        cmp #$7c                ; '|' after a %v ?
        jne formatNumber
        ; %vN|width  – parse the width
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
        ; width = width * 10 + digit  (clamped to 15)
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
:       sta formatWidth
formatWidthNext:
        jsr formatNext
        jmp formatParseWidth

; ---------------------------------------------------------------------
; formatLookup
; Dispatch on the format letter and resolve the replacement text.
; ---------------------------------------------------------------------
formatLookup:
        stz _formatText
        stz _formatText+1
        stz _formatRecursive
        lda _formatInvalid
        beq :+
        jmp formatScan          ; bad index → ignore the whole token
:       lda _formatCode
        cmp #$73                ; 's'
        jeq formatString
        cmp #$77                ; 'w'
        jeq formatWord
        cmp #$30                ; '0'  (object)
        jeq formatObject
        cmp #$67                ; 'g'
        beq @global
        cmp #$6d                ; 'm'
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
        jmp formatScan          ; nothing to insert
:       lda _formatRecursive
        jne formatPush          ; the replacement may itself contain % codes

; ---------------------------------------------------------------------
; formatCopyText
; Copy a resolved string that does *not* need further expansion.
; Replacement bytes are treated as pure literals (even if they contain
; '%' or '\').
; ---------------------------------------------------------------------
formatCopyText:
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts
:       lda _formatTextBank
        sta $00
        lda _formatText
        sta ptr1
        lda _formatText+1
        sta ptr1+1
        ldx _formatBuffered
        ldy #0
        jsr formatBeginRun

formatCopyLiteral:
        lda (ptr1),y
        beq formatCopyLiteralEnd
        sta _formatBuffer,x
        inx
        iny
        bne :+
        inc ptr1+1
:       cpx formatRunLimit
        bne formatCopyLiteral
        jsr formatEndRun
        cpx #50
        bne :+
        phy
        jsr formatFlush
        ply
        ldx #0
:       lda _formatRemaining
        ora _formatRemaining+1
        beq :+
        jsr formatBeginRun
        bra formatCopyLiteral
:       rts

formatCopyLiteralEnd:
        jsr formatEndRun
        jmp formatScan

; ---------------------------------------------------------------------
; formatMessage
; Resolve a message (or global) through the engine's bank-5 API.
; The packed LOGICFile layout is verified at compile time by the C side.
;
; On entry: A = logic number (0 for globals)
; On exit:  AX = pointer to the message text, or 0 if missing
;           _formatTextBank is set
; ---------------------------------------------------------------------
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
        jsr _trampoline         ; call into bank 5
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
        asl                     ; index * 2
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

; ---------------------------------------------------------------------
; formatTimes5
; Multiplies A by 5 (used for 5-byte string/word/object entries).
; Result is in A (low) / formatOffsetHi (high).
; ---------------------------------------------------------------------
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
:       rts

; ---------------------------------------------------------------------
; formatString  (%sN)
; N is 0-11.  Strings live in bank 7, 40 bytes apart.
; ---------------------------------------------------------------------
formatString:
        lda _formatIndex
        cmp #12
        jcs formatScan
        jsr formatTimes5
        .repeat 3
        asl
        rol formatOffsetHi
        .endrepeat              ; * 8  → total * 40
        clc
        adc #<_string
        sta _formatText
        lda formatOffsetHi
        adc #>_string
        sta _formatText+1
        lda #7
        sta _formatTextBank
        inc _formatRecursive    ; strings may contain further % codes
        jmp formatResolved

; ---------------------------------------------------------------------
; formatWord  (%wN)
; N is 1-10.  Word text lives in bank 7.
; ---------------------------------------------------------------------
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
        .endrepeat              ; * 16  → total * 80
        clc
        adc #<_b7WordText
        sta _formatText
        lda formatOffsetHi
        adc #>_b7WordText
        sta _formatText+1
        lda #7
        sta _formatTextBank
        jmp formatResolved

; ---------------------------------------------------------------------
; formatObject  (%0N  – the letter is actually '0')
; N is 1-100.  Objects live in bank 13.
; ---------------------------------------------------------------------
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

; ---------------------------------------------------------------------
; formatPush
; Push a new recursion frame (for a message or string that itself
; contains format codes).  Maximum depth is 7.
; Also detects cycles so we never infinite-loop on circular %m references.
; ---------------------------------------------------------------------
formatPush:
        ldx formatDepth
        cpx #7
        bcc :+
        jmp formatScan          ; too deep – ignore
:       ldx #0
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
        jmp formatScan          ; cycle detected
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

; ---------------------------------------------------------------------
; formatPop
; Restore the previous recursion frame and continue scanning.
; ---------------------------------------------------------------------
formatPop:
        ldx formatDepth
        bne :+
        rts                     ; back at the top level – we are done
:       dex
        stx formatDepth
        lda formatCursorsLo,x
        sta formatCursor
        lda formatCursorsHi,x
        sta formatCursor+1
        lda formatBanks,x
        sta formatBank
        jmp formatScan

; ---------------------------------------------------------------------
; formatNumber
; Formats a variable (%vN or %vN|width) as a decimal number with
; optional zero-padding.
; ---------------------------------------------------------------------
formatNumber:
        lda _formatInvalid
        beq :+
        jmp formatScan
:       lda _var
        sta ptr1
        lda _var+1
        sta ptr1+1
        ldy _formatIndex
        lda (ptr1),y            ; the variable value (0-255)

        ; Convert to three decimal digits
        ldx #0
formatHundreds:
        cmp #100
        bcc :+
        sbc #100
        inx
        jmp formatHundreds
:       stx formatDigits+2
        ldx #0
formatTens:
        cmp #10
        bcc :+
        sbc #10
        inx
        jmp formatTens
:       sta formatDigits
        stx formatDigits+1

        ; How many significant digits do we have?
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
        stx formatWidth         ; no explicit width → use natural width
:       lda formatWidth
        cmp formatCount
        bcc :+
        lda formatCount
:       sta formatUsed

; Emit leading zeros if a width was requested
formatPadding:
        lda formatWidth
        cmp formatUsed
        jeq formatEmitDigits
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts
:       lda #$30
        jsr formatAppend
        dec formatWidth
        jmp formatPadding

; Emit the significant digits (most-significant first)
formatEmitDigits:
        lda formatUsed
        jeq formatNumberDone
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts
:       dec formatUsed
        ldx formatUsed
        lda formatDigits,x
        clc
        adc #$30
        jsr formatAppend
        jmp formatEmitDigits

formatNumberDone:
        jmp formatScan