.ifndef TELEPORT_SCAN_INC
TELEPORT_SCAN_INC = 1
; teleportScan.s - How often does each query word occur in this text?
;
; Decrypts a room logic's message text (XOR "Avis Durgan") as it streams by
; in chunks, splits it into words and counts, per query word, the text words
; that match. Words are runs of ASCII letters and digits, folded to lowercase;
; apostrophes are dropped (king's -> kings); anything else ends a word. A text
; word matches a query word when equal, or when one is the other plus 's'
; (king/kings). As in Meilisearch, the last word of each stop is also a
; prefix: "tp cast" finds "castle". Text words over 15 characters never
; match. Query words are offsets into the command line (teleportGrammar.s).
.importzp ptr1, ptr2, tmp1, tmp2, tmp3
.import popax, _avisDurgan
.export _bFTeleportHits

TELEPORT_KEY_LENGTH = 11

.segment "BANKRAM0F"
_bFTeleportHits: .res TELEPORT_MAX_WORDS ; saturating at 255
bFScanKey: .res 1
bFScanLength: .res 1            ; TELEPORT_WORD_SIZE marks an overlong word
bFScanWord: .res TELEPORT_WORD_SIZE
bFScanQuery: .res 1
bFScanFlags: .res TELEPORT_MAX_WORDS ; per query word: SCAN_FIRST, SCAN_LAST
SCAN_LAST = 1                   ; last word of its stop: also a prefix
SCAN_FIRST = 2                  ; first word of its stop
bFScanSaveY: .res 1

TELEPORT_ASCII
; Starts a room's message text; marks each stop's first and last word.
bFTeleportScanReset:
    stz bFScanKey
    stz bFScanLength
    ldx #TELEPORT_MAX_WORDS - 1
@clear:
    stz _bFTeleportHits,x
    stz bFScanFlags,x
    dex
    bpl @clear
    ldx _bFTeleportStopCount
@stop:
    dex
    bmi @done
    lda _bFTeleportStopFirst + 1,x
    cmp _bFTeleportStopFirst,x
    beq @stop                   ; a number: no words
    tay
    lda #SCAN_LAST
    sta bFScanFlags - 1,y
    ldy _bFTeleportStopFirst,x
    lda bFScanFlags,y
    ora #SCAN_FIRST
    sta bFScanFlags,y
    bra @stop
@done:
    rts

; Scans A bytes (1-255) at ptr1 (low RAM). Decryption and word state carry
; over from the previous call.
bFTeleportScan:
    sta tmp1
    jsr bFTeleportLinePointer2
    ldy #0
@byte:
    lda (ptr1),y
    ldx bFScanKey
    eor _avisDurgan,x
    inx
    cpx #TELEPORT_KEY_LENGTH
    bne @key
    ldx #0
@key:
    stx bFScanKey
    cmp #$27                    ; apostrophe: drop it
    beq @next
    cmp #'A'
    bcc @digit
    cmp #'Z' + 1
    bcs @lower
    ora #$20
    bra @append
@lower:
    cmp #'a'
    bcc @boundary
    cmp #'z' + 1
    bcs @boundary
    bra @append
@digit:
    cmp #'0'
    bcc @boundary
    cmp #'9' + 1
    bcs @boundary
@append:
    ldx bFScanLength
    cpx #TELEPORT_WORD_SIZE
    beq @next
    sta bFScanWord,x
    inc bFScanLength
    bra @next
@boundary:
    jsr bFScanEndWord
@next:
    iny
    cpy tmp1
    bne @byte
    rts

; Ends the room's message text.
bFTeleportScanEnd:
    jsr bFTeleportLinePointer2
    ; Fall through.

; Counts the current word against every query word, then clears it.
; Preserves Y.
bFScanEndWord:
    lda bFScanLength
    beq @exit
    cmp #TELEPORT_WORD_SIZE
    beq @clear
    sty bFScanSaveY
    ldx #0
@query:
    cpx _bFTeleportWords
    beq @restore
    jsr bFScanMatch
    bcc @next
    lda _bFTeleportHits,x
    inc a
    beq @next
    sta _bFTeleportHits,x
@next:
    inx
    bra @query
@restore:
    ldy bFScanSaveY
@clear:
    stz bFScanLength
@exit:
    rts

; Does the current word match query word X: equal, one the other plus 's',
; or, for the last word of a stop, starting with it? Carry set if so.
; Preserves X.
bFScanMatch:
    lda bFScanFlags,x
    lsr a                       ; SCAN_LAST
    bcc @whole
    lda bFScanLength
    cmp _bFTeleportWordLength,x
    bcc @whole
    lda _bFTeleportWordLength,x
    bra @compare
@whole:
    lda bFScanLength
    sec
    sbc _bFTeleportWordLength,x
    beq @same
    cmp #1
    beq @wordPlural
    cmp #$FF
    bne @no
    lda _bFTeleportWordStart,x  ; query = word + 's'
    clc
    adc bFScanLength
    tay
    lda (ptr2),y
    cmp #'s'
    bne @no
@same:
    lda bFScanLength
    bra @compare
@wordPlural:                    ; word = query + 's'
    ldy bFScanLength
    lda bFScanWord - 1,y
    cmp #'s'
    bne @no
    lda _bFTeleportWordLength,x
@compare:                       ; A characters of the query and the word
    sta tmp2
    stx bFScanQuery
    lda _bFTeleportWordStart,x
    tay
    ldx #0
@char:
    lda (ptr2),y
    cmp bFScanWord,x
    bne @differ
    iny
    inx
    cpx tmp2
    bne @char
    ldx bFScanQuery
    sec
    rts
@differ:
    ldx bFScanQuery
@no:
    clc
    rts
.popcharmap
.endif
