; =====================================================================
; stringFormatAsm.s
;
; Expands AGI-style format strings into a destination buffer.
; Scalar state lives in the externally-declared ZP_TMP_* slots.
; The 50-byte output buffer and the recursion-frame arrays stay in BSS.
;
; Full C equivalent is shown in comments next to the matching
; assembly so every statement can be followed line-by-line.
; =====================================================================

.macpack longbranch

.import _b5GetLogicFile, _var, _currentLog
.global _trampoline
.import _string, _b7WordText, _numInputWords, _bDObjects, _bDNumObjects
.importzp ptr1, ptr2, ptr4, tmp4, sp
.import incsp6, pushax
.export _expandFormatCodes, _formatIndex, _formatTextBank

; ---------------------------------------------------------------------
; Zero-page assignments (ZP_TMP_* already declared elsewhere)
; ---------------------------------------------------------------------
_formatSource           = ZP_TMP_2      ; const char *source
_formatDestination      = ZP_TMP_3      ; char *destination
_formatRemaining        = ZP_TMP_4      ; uint16_t remaining
_formatText             = ZP_TMP_5      ; const char *replacement
formatCursor            = ZP_TMP_6      ; const char *cursor

_formatSourceBank       = ZP_TMP_7
_formatDestinationBank  = ZP_TMP_7+1
_formatCode             = ZP_TMP_8      ; char code
_formatIndex            = ZP_TMP_8+1    ; uint8_t index
_formatInvalid          = ZP_TMP_9
_formatTextBank         = ZP_TMP_9+1
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
formatDigits            = ZP_TMP_19     ; uint8_t digits[3]

.segment "BSS"
_formatBuffered:        .res 1
_formatBuffer:          .res 50
formatLogicFile:        .res 9
formatBasesLo:          .res 8
formatBasesHi:          .res 8
formatBanks:            .res 8
formatCursorsLo:        .res 8
formatCursorsHi:        .res 8

.segment "CODE"

; ---------------------------------------------------------------------
; char formatPeek(void)
; {
;     set_bank(formatBank);          // sta $00
;     ptr1 = formatCursor;           // lda/sta ptr1
;     return *ptr1;                  // lda (ptr1)
; }
; ---------------------------------------------------------------------
formatPeek:
        lda formatBank              ; A = formatBank
        sta $00                     ; set_bank(formatBank)
        lda formatCursor            ; A = formatCursor.lo
        sta ptr1                    ; ptr1.lo = A
        lda formatCursor+1          ; A = formatCursor.hi
        sta ptr1+1                  ; ptr1.hi = A
        lda (ptr1)                  ; A = *ptr1
        rts

; ---------------------------------------------------------------------
; void formatNext(void)
; {
;     formatCursor++;
; }
; ---------------------------------------------------------------------
formatNext:
        inc formatCursor            ; formatCursor.lo++
        bne :+                      ; if no carry, done
        inc formatCursor+1          ; else formatCursor.hi++
:       rts

; ---------------------------------------------------------------------
; void formatAppend(char c)          // c arrives in A
; {
;     buffer[buffered] = c;
;     buffered++;
;     remaining--;
;     if (buffered == 50)
;         formatFlush();
; }
; ---------------------------------------------------------------------
formatAppend:
        ldx _formatBuffered         ; X = buffered
        sta _formatBuffer,x         ; buffer[buffered] = c
        inc _formatBuffered         ; buffered++
        lda _formatRemaining        ; A = remaining.lo
        bne :+                      ; if lo != 0 skip
        dec _formatRemaining+1      ; else remaining.hi--
:       dec _formatRemaining        ; remaining.lo--
        lda _formatBuffered         ; A = buffered
        cmp #50
        bne :+                      ; if buffered != 50 skip
        jsr formatFlush             ; formatFlush()
:       rts

formatBuffered = _formatBuffered

; =====================================================================
; void expandFormatCodes(uint16_t dstMax,  // AX
;                        uint8_t dstBank, char *dst,
;                        uint8_t srcBank, const char *src)
; =====================================================================
.export _expandFormatCodes
_expandFormatCodes:
        ; remaining = dstMax
        sta _formatRemaining        ; remaining.lo = A
        stx _formatRemaining+1      ; remaining.hi = X
        ora _formatRemaining+1      ; test if both zero
        bne :+
        jmp incsp6                  ; if (dstMax == 0) { clean stack; return; }

:       lda $00
        sta formatPreviousBank      ; previousBank = current bank

        ; Pull parameters from cc65 software stack
        ; (sp)[0] = dstBank
        ; (sp)[1..2] = dst
        ; (sp)[3] = srcBank
        ; (sp)[4..5] = src
        ldy #0
        lda (sp),y
        sta _formatDestinationBank  ; destinationBank = stack[0]
        iny
        lda (sp),y
        sta _formatDestination      ; destination.lo = stack[1]
        iny
        lda (sp),y
        sta _formatDestination+1    ; destination.hi = stack[2]
        iny
        lda (sp),y
        sta _formatSourceBank       ; sourceBank = stack[3]
        iny
        lda (sp),y
        sta _formatSource           ; source.lo = stack[4]
        iny
        lda (sp),y
        sta _formatSource+1         ; source.hi = stack[5]

        ; remaining = dstMax - 1;   // reserve NUL
        lda _formatRemaining
        bne :+
        dec _formatRemaining+1
:       dec _formatRemaining

        stz _formatBuffered         ; buffered = 0
        jsr formatExpand            ; formatExpand()
        jsr formatFlush             ; formatFlush()

        ; set_bank(destinationBank);
        ; *destination = 0;
        lda _formatDestinationBank
        sta $00
        lda _formatDestination
        sta ptr2
        lda _formatDestination+1
        sta ptr2+1
        lda #0
        sta (ptr2)                  ; *destination = '\0'

        lda formatPreviousBank
        sta $00                     ; restore caller's bank
        jmp incsp6                  ; clean 6 bytes from software stack

; ---------------------------------------------------------------------
; void formatFlush(void)
; {
;     if (buffered == 0) return;
;     saved = current_bank;
;     set_bank(destinationBank);
;     memcpy(destination, buffer, buffered);
;     destination += buffered;
;     buffered = 0;
;     set_bank(saved);
; }
; ---------------------------------------------------------------------
formatFlush:
        ldy _formatBuffered         ; Y = buffered
        beq @done                   ; if (buffered == 0) return
        lda $00
        pha                         ; save current bank
        lda _formatDestinationBank
        sta $00                     ; set_bank(destinationBank)
        lda _formatDestination
        sta ptr2
        lda _formatDestination+1
        sta ptr2+1
        dey                         ; Y = buffered - 1
@copy:
        lda _formatBuffer,y         ; A = buffer[Y]
        sta (ptr2),y                ; destination[Y] = A
        dey
        bpl @copy                   ; while (Y >= 0)
        clc
        lda _formatDestination
        adc _formatBuffered
        sta _formatDestination      ; destination += buffered
        bcc :+
        inc _formatDestination+1
:       stz _formatBuffered         ; buffered = 0
        pla
        sta $00                     ; restore bank
@done:
        rts

; ---------------------------------------------------------------------
; void formatExpand(void)
; {
;     depth = 0;
;     cursor = source;
;     bases[0] = source;
;     bank = sourceBank;
;     banks[0] = sourceBank;
;     // fall into scan
; }
; ---------------------------------------------------------------------
formatExpand:
        stz formatDepth             ; depth = 0
        lda _formatSource
        sta formatCursor            ; cursor = source
        sta formatBasesLo
        lda _formatSource+1
        sta formatCursor+1
        sta formatBasesHi
        lda _formatSourceBank
        sta formatBank              ; bank = sourceBank
        sta formatBanks

; ---------------------------------------------------------------------
; scan:
; while (remaining != 0) {
;     set_bank(bank);
;     // copy a run of literal characters into the buffer
;     // stop at '\0', '%', '\\', buffer full, or remaining == 0
; }
; ---------------------------------------------------------------------
formatScan:
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts                         ; if (remaining == 0) return

:       lda formatBank
        sta $00                     ; set_bank(bank)
        lda formatCursor
        sta ptr1
        lda formatCursor+1
        sta ptr1+1
        ldx _formatBuffered         ; X = buffered (index into buffer)
        ldy #0                      ; Y = offset from ptr1
        jsr formatBeginRun          ; compute how far we may copy

; while ((c = ptr1[Y]) != 0 && c != '%' && c != '\\' && X < runLimit)
formatLiteral:
        lda (ptr1),y                ; c = ptr1[Y]
        beq formatLiteralEnd        ; if (c == 0) end of string
        cmp #$25                    ; '%'
        beq formatLiteralToken
        cmp #$5c                    ; '\\'
        beq formatLiteralEscape
        sta _formatBuffer,x         ; buffer[X] = c
        inx                         ; X++
        iny                         ; Y++
        bne :+
        inc ptr1+1                  ; handle 16-bit pointer wrap
:       cpx formatRunLimit
        bne formatLiteral           ; while (X < runLimit)
        jsr formatEndRun            ; commit the run (update remaining)
        cpx #50
        bne :+
        phy
        jsr formatFlush             ; if buffer full, flush
        ply
        ldx #0                      ; X = 0 after flush
:       lda _formatRemaining
        ora _formatRemaining+1
        beq :+
        jsr formatBeginRun
        bra formatLiteral           ; continue if still room
:       jsr formatSaveCursor
        rts

formatLiteralEnd:
        jsr formatSaveLiteral       ; commit + advance cursor
        jmp formatPop               ; end of this string → pop frame

formatLiteralToken:
        jsr formatSaveLiteral
        jmp formatToken             ; handle '%'

formatLiteralEscape:
        jsr formatSaveLiteral
        jmp formatEscape            ; handle '\\'

formatSaveLiteral:
        jsr formatEndRun
formatSaveCursor:
        ; cursor = ptr1 + Y
        tya
        clc
        adc ptr1
        sta formatCursor
        lda ptr1+1
        adc #0
        sta formatCursor+1
        rts

; ---------------------------------------------------------------------
; void formatBeginRun(void)   // X = current buffered count
; {
;     runStart = X;
;     runLimit = 50;
;     if (remaining.hi == 0) {
;         uint16_t possible = X + remaining.lo;
;         if (possible < 50)
;             runLimit = possible;
;     }
; }
; ---------------------------------------------------------------------
formatBeginRun:
        stx formatRunStart          ; runStart = X
        lda #50
        sta formatRunLimit          ; runLimit = 50
        lda _formatRemaining+1
        bne @done                   ; if remaining.hi != 0, full 50 ok
        txa
        clc
        adc _formatRemaining        ; A = X + remaining.lo
        bcs @done                   ; overflow → keep 50
        cmp #50
        bcs @done
        sta formatRunLimit          ; runLimit = X + remaining
@done:
        rts

; ---------------------------------------------------------------------
; void formatEndRun(void)     // X = new buffered count
; {
;     uint8_t added = X - runStart;
;     buffered = X;
;     remaining -= added;
; }
; ---------------------------------------------------------------------
formatEndRun:
        stx _formatBuffered         ; buffered = X
        txa
        sec
        sbc formatRunStart          ; A = X - runStart
        sta formatRunStart          ; reuse as 'added'
        sec
        lda _formatRemaining
        sbc formatRunStart
        sta _formatRemaining        ; remaining -= added
        bcs @done
        dec _formatRemaining+1
@done:
        rts

; ---------------------------------------------------------------------
; void formatEscape(void)
; {
;     cursor++;
;     if (*cursor == 0)
;         append('\\');
;     else {
;         append(*cursor);
;         cursor++;
;     }
; }
; ---------------------------------------------------------------------
formatEscape:
        jsr formatNext              ; cursor++
        jsr formatPeek              ; A = *cursor
        bne :+
        lda #$5c                    ; '\\'
        jsr formatAppend            ; append('\\')
        jmp formatPop
:       jsr formatAppend            ; append(*cursor)
        jsr formatNext              ; cursor++
        jmp formatScan

; ---------------------------------------------------------------------
; void formatToken(void)
; {
;     cursor++;
;     if (*cursor == 0) { append('%'); return; }
;     code = *cursor;
;     cursor++;
;     index = 0;
;     invalid = 0;
;     // parse digits...
; }
; ---------------------------------------------------------------------
formatToken:
        jsr formatNext              ; cursor++
        jsr formatPeek
        bne :+
        lda #$25                    ; '%'
        jsr formatAppend            ; lone '%' at end
        jmp formatPop
:       sta _formatCode             ; code = *cursor
        jsr formatNext              ; cursor++
        stz _formatIndex            ; index = 0
        stz _formatInvalid          ; invalid = 0

; while (isdigit(*cursor))
formatParseIndex:
        jsr formatPeek
        cmp #$30                    ; '0'
        jcc formatIndexDone
        cmp #$3a                    ; '9'+1
        jcs formatIndexDone
        sec
        sbc #$30
        sta formatDigit             ; digit = *cursor - '0'
        lda _formatInvalid
        jne formatIndexNext         ; if already invalid, just consume
        lda _formatIndex
        cmp #25
        jcc formatIndexAccumulate
        jne formatIndexInvalid
        lda formatDigit
        cmp #6                      ; 25*10+5 = 255; digit>5 → overflow
        jcs formatIndexInvalid
formatIndexAccumulate:
        ; index = index * 10 + digit
        lda _formatIndex
        asl                         ; *2
        sta formatValue
        asl                         ; *4
        asl                         ; *8
        clc
        adc formatValue             ; *10
        adc formatDigit
        sta _formatIndex
        jmp formatIndexNext
formatIndexInvalid:
        inc _formatInvalid          ; invalid = 1
formatIndexNext:
        jsr formatNext              ; cursor++
        jmp formatParseIndex

formatIndexDone:
        stz formatHasWidth          ; hasWidth = 0
        stz formatWidth             ; width = 0
        ldx _formatCode
        cpx #$76                    ; 'v'
        jne formatLookup
        cmp #$7c                    ; '|'
        jne formatNumber
        ; %v N | width
        inc formatHasWidth          ; hasWidth = 1
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
        jcs formatWidthNext         ; already at max
        ; width = min(15, width*10 + digit)
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
; lookup the replacement for code/index
; ---------------------------------------------------------------------
formatLookup:
        stz _formatText
        stz _formatText+1           ; text = NULL
        stz _formatRecursive        ; recursive = 0
        lda _formatInvalid
        beq :+
        jmp formatScan              ; bad index → ignore token
:       lda _formatCode
        cmp #$73                    ; 's'
        jeq formatString
        cmp #$77                    ; 'w'
        jeq formatWord
        cmp #$30                    ; '0' (object)
        jeq formatObject
        cmp #$67                    ; 'g'
        beq @global
        cmp #$6d                    ; 'm'
        beq @message
        jmp formatScan
@global:
        lda #0                      ; logic 0
        bra @lookup
@message:
        inc _formatRecursive        ; recursive = 1
        lda _currentLog
@lookup:
        jsr formatMessage           ; AX = message pointer
        sta _formatText
        stx _formatText+1

formatResolved:
        lda _formatText
        ora _formatText+1
        bne :+
        jmp formatScan              ; NULL → nothing to insert
:       lda _formatRecursive
        jne formatPush              ; needs further expansion

; ---------------------------------------------------------------------
; Copy a non-recursive replacement literally
; (even if it contains '%' or '\\')
; ---------------------------------------------------------------------
formatCopyText:
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts
:       lda _formatTextBank
        sta $00                     ; set_bank(textBank)
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
; formatMessage: call bank-5 _b5GetLogicFile via trampoline
; On entry A = logic number
; On exit  AX = pointer to message text (or 0)
;          textBank set from the LOGICFile
; ---------------------------------------------------------------------
formatMessage:
        pha                         ; save logic number
        lda #<formatLogicFile
        ldx #>formatLogicFile
        jsr pushax                  ; push &formatLogicFile
        ldy #5
        sty tmp4                    ; trampoline bank argument
        ldy #<_b5GetLogicFile
        sty ptr4
        ldy #>_b5GetLogicFile
        sty ptr4+1
        pla                         ; A = logic number
        ldx #0
        jsr _trampoline             ; call into bank 5
        lda formatLogicFile+8
        sta _formatTextBank         ; textBank = file.bank
        lda formatLogicFile+5
        ora formatLogicFile+6
        beq @missing                ; no message table
        lda _formatIndex
        beq @missing
        cmp formatLogicFile+4       ; compare with message count
        beq :+
        bcs @missing
:
        dec a                       ; index is 1-based
        stz formatOffsetHi
        asl                         ; *2
        rol formatOffsetHi
        clc
        adc formatLogicFile+5       ; + table base
        sta ptr1
        lda formatOffsetHi
        adc formatLogicFile+6
        sta ptr1+1
        lda _formatTextBank
        sta $00
        ldy #1
        lda (ptr1),y                ; high byte of message pointer
        tax
        lda (ptr1)                  ; low byte
        rts
@missing:
        lda #0
        tax
        rts

; A * 5 → A (lo) / formatOffsetHi (hi)
formatTimes5:
        stz formatOffsetHi
        sta formatValue
        asl
        rol formatOffsetHi          ; *2
        asl
        rol formatOffsetHi          ; *4
        clc
        adc formatValue             ; *5
        bcc :+
        inc formatOffsetHi
:       rts

; %sN  (bank 7, 40-byte entries, recursive)
formatString:
        lda _formatIndex
        cmp #12
        jcs formatScan
        jsr formatTimes5            ; index * 5
        .repeat 3
        asl
        rol formatOffsetHi
        .endrepeat                  ; *8 → total *40
        clc
        adc #<_string
        sta _formatText
        lda formatOffsetHi
        adc #>_string
        sta _formatText+1
        lda #7
        sta _formatTextBank         ; bank = 7
        inc _formatRecursive
        jmp formatResolved

; %wN  (bank 7)
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
        dec                         ; 1-based → 0-based
        jsr formatTimes5
        .repeat 4
        asl
        rol formatOffsetHi
        .endrepeat                  ; *16 → total *80
        clc
        adc #<_b7WordText
        sta _formatText
        lda formatOffsetHi
        adc #>_b7WordText
        sta _formatText+1
        lda #7
        sta _formatTextBank
        jmp formatResolved

; %0N  (bank 13)
formatObject:
        lda _formatIndex
        jeq formatScan
        cmp #101
        jcs formatScan
        lda #13
        sta $00                     ; set_bank(13) to read object table
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
        sta _formatText             ; text = object name pointer
        ldy #1
        lda (ptr1),y
        sta _formatText+1
        lda #13
        sta _formatTextBank
@invalid:
        lda formatBank
        sta $00                     ; restore previous bank
        jmp formatResolved

; ---------------------------------------------------------------------
; Push a recursion frame (max depth 7) with cycle detection
; ---------------------------------------------------------------------
formatPush:
        ldx formatDepth
        cpx #7
        bcc :+
        jmp formatScan              ; too deep
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
        jmp formatScan              ; cycle → ignore
formatCycleNext:
        cpx formatDepth
        jeq formatSaveFrame
        inx
        jmp formatCheckCycle
formatSaveFrame:
        lda formatCursor
        sta formatCursorsLo,x       ; save current cursor
        lda formatCursor+1
        sta formatCursorsHi,x
        inx
        stx formatDepth             ; depth++
        lda _formatText
        sta formatBasesLo,x
        sta formatCursor            ; cursor = text
        lda _formatText+1
        sta formatBasesHi,x
        sta formatCursor+1
        lda _formatTextBank
        sta formatBanks,x
        sta formatBank              ; bank = textBank
        jmp formatScan

; ---------------------------------------------------------------------
; Pop a recursion frame
; ---------------------------------------------------------------------
formatPop:
        ldx formatDepth
        bne :+
        rts                         ; depth == 0 → done
:       dex
        stx formatDepth             ; depth--
        lda formatCursorsLo,x
        sta formatCursor            ; restore cursor
        lda formatCursorsHi,x
        sta formatCursor+1
        lda formatBanks,x
        sta formatBank              ; restore bank
        jmp formatScan

; ---------------------------------------------------------------------
; Format a variable as decimal with optional zero-padding
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
        lda (ptr1),y                ; value = var[index]

        ; split into hundreds / tens / units
        ldx #0
formatHundreds:
        cmp #100
        bcc :+
        sbc #100
        inx
        jmp formatHundreds
:       stx formatDigits+2          ; digits[2] = hundreds
        ldx #0
formatTens:
        cmp #10
        bcc :+
        sbc #10
        inx
        jmp formatTens
:       sta formatDigits            ; digits[0] = units
        stx formatDigits+1          ; digits[1] = tens

        ; count = number of significant digits
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
        stx formatWidth             ; no explicit width → natural width
:       lda formatWidth
        cmp formatCount
        bcc :+
        lda formatCount
:       sta formatUsed              ; used = min(width, count)

; leading zeros
formatPadding:
        lda formatWidth
        cmp formatUsed
        jeq formatEmitDigits
        lda _formatRemaining
        ora _formatRemaining+1
        bne :+
        rts
:       lda #$30                    ; '0'
        jsr formatAppend
        dec formatWidth
        jmp formatPadding

; significant digits, MSD first
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
        adc #$30                    ; digit + '0'
        jsr formatAppend
        jmp formatEmitDigits

formatNumberDone:
        jmp formatScan