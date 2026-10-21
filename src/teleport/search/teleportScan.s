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
; prefix: "tp cast" finds "castle". Otherwise, as in Meilisearch, a query
; word of 5 letters or more allows one typo, of 9 or more two: a letter
; changed, missing, extra, or two neighbours swapped each count one, and a
; wrong first letter counts two. _bFTeleportTypos keeps each query word's
; fewest typos ($FF: no match). For proximity, also Meilisearch's, every
; text word has a position (a sentence end adds 4) and _bFTeleportPairs
; keeps, for each query word and the next one in its stop, how close they
; came: in order, distance - 1; reversed, distance; at most 3 (not close).
; Two neighbours of a stop joined may also equal a text word (Meilisearch's
; 2-grams: "bean stalk" finds "beanstalk"): a match for both, adjacent, and
; costing one typo.
; Text words over 15 characters never match.
; Query words are offsets into the command line (teleportGrammar.s).
.importzp ptr1, ptr2, ptr4, tmp1, tmp2, tmp3, tmp4
.import popax, _avisDurgan
.export _bFTeleportHits, _bFTeleportTypos, _bFTeleportPairs
TELEPORT_FAR = 3                ; pair cost: not close
TELEPORT_SENTENCE = 4           ; positions a sentence end adds

TELEPORT_KEY_LENGTH = 11

.segment "BANKRAM0F"
_bFTeleportHits: .res TELEPORT_MAX_WORDS ; saturating at 255
_bFTeleportTypos: .res TELEPORT_MAX_WORDS ; fewest typos of a match, $FF none
_bFTeleportPairs: .res TELEPORT_MAX_WORDS ; word and the next: 0 adjacent .. 3 far
bFScanPosition: .res 2          ; the current text word's position
bFScanLastLow: .res TELEPORT_MAX_WORDS ; each query word's last match
bFScanLastHigh: .res TELEPORT_MAX_WORDS
bFEditEnd: .res 1               ; the query word's end in the line
bFEditPrefix: .res 1            ; nonzero: the word may run on past it
bFEditLeft: .res 1              ; edits still allowed
bFEditBudget: .res 1
bFEditRest: .res 1
bFScanKey: .res 1
bFScanLength: .res 1            ; TELEPORT_WORD_SIZE marks an overlong word
bFScanWord: .res TELEPORT_WORD_SIZE
bFScanQuery: .res 1
bFScanFlags: .res TELEPORT_MAX_WORDS ; per query word: SCAN_FIRST, SCAN_LAST
; Query word lists, in order, as word + 1 (0 ends): per first character
; (0-9, then a-z) and of 9+ letters, which may begin with any (two typos).
bFScanChain: .res TELEPORT_MAX_WORDS ; the next word with the same first character
bFScanLongChain: .res TELEPORT_MAX_WORDS ; the next of 9+ letters
bFScanStarts: .res 36           ; each list's first word
bFScanJoined: .res TELEPORT_WORD_SIZE ; nonzero: two stop neighbours make a word this long
bFScanLong: .res 1
SCAN_TABLES = 36 + TELEPORT_WORD_SIZE + 1 ; from bFScanStarts on
bFScanLetterNext: .res 1        ; the walk through both lists
bFScanLongNext: .res 1
SCAN_LAST = 1                   ; last word of its stop: also a prefix
SCAN_FIRST = 2                  ; first word of its stop
bFScanSaveY: .res 1

TELEPORT_ASCII
; Starts a room's message text; marks each stop's first and last word.
bFTeleportScanReset:
    stz bFScanKey
    stz bFScanLength
    ldx #TELEPORT_MAX_WORDS - 1
    stz bFScanPosition
    stz bFScanPosition + 1
@clear:
    stz _bFTeleportHits,x
    stz bFScanFlags,x
    lda #$FF
    sta _bFTeleportTypos,x
    lda #TELEPORT_FAR
    sta _bFTeleportPairs,x
    dex
    bpl @clear
    jsr bFTeleportLinePointer2  ; the query word lists
    ldx #SCAN_TABLES - 1
@tables:
    stz bFScanStarts,x
    dex
    bpl @tables
    ldx _bFTeleportWords
@word:
    dex
    bmi @words
    lda _bFTeleportWordLength,x
    cmp #9
    bcc @short
    lda bFScanLong
    sta bFScanLongChain,x
    txa
    inc a
    sta bFScanLong
@short:
    ldy _bFTeleportWordStart,x  ; 0-9 then a-z
    lda (ptr2),y
    cmp #'a'
    bcc @digit
    sbc #'a' - 10
    bra @first
@digit:
    sbc #'0' - 1                ; carry clear
@first:
    tay
    lda bFScanStarts,y
    sta bFScanChain,x
    txa
    inc a
    sta bFScanStarts,y
    bra @word
@words:
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
    phx                         ; the stop's joinable lengths
@join:
    iny
    tya
    cmp _bFTeleportStopFirst + 1,x
    bcs @joined
    lda _bFTeleportWordLength - 1,y
    clc
    adc _bFTeleportWordLength,y
    cmp #TELEPORT_WORD_SIZE
    bcs @join
    tax
    sta bFScanJoined,x          ; nonzero: at least 2
    plx
    phx
    bra @join
@joined:
    plx
    bra @stop
@done:
    rts

; Scans A bytes (1-255) at ptr1 (low RAM). Decryption and word state carry
; over from the previous call. X holds the word's length; the key byte of
; data byte Y is (ptr4),y until Y reaches tmp3, where the key restarts.
; tmp4, the nearer of that and the end, is the only test per byte.
bFTeleportScan:
    sta tmp1
    jsr bFTeleportLinePointer2
    lda bFScanKey
    clc
    adc #<_avisDurgan
    sta ptr4
    lda #>_avisDurgan
    adc #0
    sta ptr4 + 1
    lda #TELEPORT_KEY_LENGTH
    sec
    sbc bFScanKey
    sta tmp3
    ldx bFScanLength
    ldy #0
    clc
    bra @limit
@other:
    cmp #'A'
    bcc @digit
    cmp #'Z' + 1
    bcs @boundary
    ora #$20
    bra @append
@digit:
    cmp #'0'
    bcc @apostrophe
    cmp #'9' + 1
    bcc @append
    bra @boundary
@apostrophe:
    cmp #$27                    ; drop it
    beq @next
@boundary:
    cpx #0
    beq @mark                   ; no word before it
    stx bFScanLength
    ldx bFScanWord
    pha
    lda bFScanLong
    bne @word
    cpx #'a'
    bcc @word                   ; a digit
    lda bFScanStarts + 10 - 'a',x
    bne @word
    inc bFScanPosition          ; no query word starts like it: next
    bne @skipped
    inc bFScanPosition + 1
    bra @skipped
@word:
    jsr bFScanEndWord
@skipped:
    ldx #0
    pla
@mark:
    cmp #' '                    ; the most common
    beq @next
    cmp #0                      ; a sentence or message ends: far apart
    beq @sentence
    cmp #'.'
    beq @sentence
    cmp #'!'
    beq @sentence
    cmp #'?'
    bne @next
@sentence:
    lda bFScanPosition
    clc
    adc #TELEPORT_SENTENCE
    sta bFScanPosition
    bcc @next
    inc bFScanPosition + 1
    bra @next
@limit:                         ; carry set: tmp3 is past 255
    lda tmp1
    bcs @far
    cmp tmp3
    bcc @far
    lda tmp3
@far:
    sta tmp4
@byte:
    lda (ptr1),y
    eor (ptr4),y
    cmp #'a'                    ; lowercase first: the most common
    bcc @other
    cmp #'z' + 1
    bcs @boundary
@append:
    cpx #TELEPORT_WORD_SIZE
    beq @next
    sta bFScanWord,x
    inx
@next:
    iny
    cpy tmp4
    bne @byte
    cpy tmp3
    bne @end
    lda ptr4                    ; the key restarts: 11 bytes back
    sec
    sbc #TELEPORT_KEY_LENGTH
    sta ptr4
    bcs @page
    dec ptr4 + 1
@page:
    tya
    clc
    adc #TELEPORT_KEY_LENGTH
    sta tmp3
    cpy tmp1
    beq @end
    cpy tmp3                    ; carry set: tmp3 wrapped past 255
    bra @limit
@end:
    stx bFScanLength
    tya                         ; the key's place: count - restart + 11
    sec
    sbc tmp3
    clc
    adc #TELEPORT_KEY_LENGTH
    sta bFScanKey
    rts
; Ends the room's message text.
bFTeleportScanEnd:
    jsr bFTeleportLinePointer2
    ; Fall through.

; Counts the current word against the query words it can match, then clears it.
; Preserves Y.
bFScanEndWord:
    lda bFScanLength
    beq @exit
    cmp #TELEPORT_WORD_SIZE
    beq @clear
    jsr bFScanStart             ; only query words starting like it, and
    sta bFScanLetterNext        ; those of 9+ letters
    ora bFScanLong
    beq @clear                  ; none: nor can it join two
    sty bFScanSaveY
    lda bFScanLong
    bne @both
    lda bFScanLetterNext        ; one list
@chain:
    dec a
    tax
    jsr bFScanCount
    lda bFScanChain,x
    bne @chain
    bra @restore
@both:
    sta bFScanLongNext
@walk:                          ; both lists, in order, each word once
    lda bFScanLetterNext
    beq @long
    ldx bFScanLongNext
    beq @letter
    cmp bFScanLongNext
    bcc @letter
    bne @long
    lda bFScanLongChain - 1,x   ; in both
    sta bFScanLongNext
    lda bFScanLetterNext
@letter:
    tax
    lda bFScanChain - 1,x
    sta bFScanLetterNext
    bra @count
@long:
    ldx bFScanLongNext
    beq @restore
    lda bFScanLongChain - 1,x
    sta bFScanLongNext
@count:
    dex
    jsr bFScanCount
    bra @walk
@restore:
    ldx bFScanLength
    lda bFScanJoined,x
    beq @unjoined
    jsr bFScanJoins
@unjoined:
    ldy bFScanSaveY
@clear:
    stz bFScanLength
    inc bFScanPosition
    bne @exit
    inc bFScanPosition + 1
@exit:
    rts

; The first query word + 1 (0 none) starting like the current word.
bFScanStart:
    ldx bFScanWord
    cpx #'a'
    bcs @letter
    lda bFScanStarts - '0',x
    rts
@letter:
    lda bFScanStarts + 10 - 'a',x
    rts

; Counts a match of the current word for query word X, if it is one.
; Preserves X.
bFScanCount:
    jsr bFScanMatch
    lda #0                      ; exact: no typos
    bcs @hit
    jsr bFScanTypos
    bcc @done
@hit:
    cmp _bFTeleportTypos,x
    bcs @count
    sta _bFTeleportTypos,x
@count:
    jsr bFScanNear
    lda _bFTeleportHits,x
    inc a
    beq @done
    sta _bFTeleportHits,x
@done:
    rts

; Does the current word join two neighbours of a stop? Each such pair (word
; X - 1, X) counts a match for both, adjacent, costing one typo. The first
; word starts like it.
bFScanJoins:
    jsr bFScanStart
    beq @done
@chain:
    tax                         ; the pair's second word
    jsr bFScanJoin
    lda bFScanChain - 1,x
    bne @chain
@done:
    rts

; Pair (X - 1, X), if X is a query word in the same stop. Preserves X.
bFScanJoin:
    cpx _bFTeleportWords
    bcs @done
    lda bFScanFlags,x
    and #SCAN_FIRST
    bne @done                   ; X starts a stop
    lda _bFTeleportWordLength - 1,x
    clc
    adc _bFTeleportWordLength,x
    cmp bFScanLength
    bne @done
    dex
    lda #0
    jsr bFScanSegment           ; the first word, then the second after it
    inx
    bcc @done
    lda _bFTeleportWordLength - 1,x
    jsr bFScanSegment
    bcc @done
    stz _bFTeleportPairs - 1,x  ; adjacent
    stz _bFTeleportTypos,x
    lda _bFTeleportTypos - 1,x  ; one typo for the pair
    cmp #2
    bcc @typo
    lda #1
    sta _bFTeleportTypos - 1,x
@typo:
    dex
    jsr @hit
    inx
@hit:                           ; a match for word X, here
    lda bFScanPosition
    sta bFScanLastLow,x
    lda bFScanPosition + 1
    sta bFScanLastHigh,x
    lda _bFTeleportHits,x
    inc a
    beq @done
    sta _bFTeleportHits,x
@done:
    rts

; Is query word X the current word's letters from A on? Carry set if so.
; Preserves X.
bFScanSegment:
    stx bFScanQuery
    tax
    ldy bFScanQuery
    lda _bFTeleportWordLength,y
    sta bFEditRest
    lda _bFTeleportWordStart,y
    tay
@char:
    lda (ptr2),y
    cmp bFScanWord,x
    bne @no
    iny
    inx
    dec bFEditRest
    bne @char
    ldx bFScanQuery
    sec
    rts
@no:
    ldx bFScanQuery
    clc
    rts

; Query word X matches here: how close did its stop neighbours' last
; matches come? Then this is its last match. Preserves X.
bFScanNear:
    lda bFScanFlags,x
    and #SCAN_FIRST
    bne @right
    lda _bFTeleportHits - 1,x   ; in order, after the previous word
    beq @right
    dex
    jsr @distance
    inx
    cmp #1
    bcc @left                   ; the same word matched both: adjacent
    dec a                       ; distance - 1
@left:
    cmp _bFTeleportPairs - 1,x
    bcs @right
    sta _bFTeleportPairs - 1,x
@right:
    lda bFScanFlags,x
    lsr a                       ; SCAN_LAST: no next word
    bcs @here
    lda _bFTeleportHits + 1,x   ; reversed, after the next word
    beq @here
    inx
    jsr @distance
    dex
    cmp #TELEPORT_FAR + 1       ; distance, at most 3
    bcc @reversed
    lda #TELEPORT_FAR
@reversed:
    cmp _bFTeleportPairs,x
    bcs @here
    sta _bFTeleportPairs,x
@here:
    lda bFScanPosition
    sta bFScanLastLow,x
    lda bFScanPosition + 1
    sta bFScanLastHigh,x
    rts
@distance:                      ; A = min(position - last match of X, 4)
    lda bFScanPosition
    sec
    sbc bFScanLastLow,x
    tay
    lda bFScanPosition + 1
    sbc bFScanLastHigh,x
    bne @far
    tya
    cmp #TELEPORT_FAR + 1
    bcc @close
@far:
    lda #TELEPORT_FAR + 1
@close:
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
; The current word within query word X's typo budget? Carry set and A =
; typos (1 or 2) if so. Preserves X.
bFScanTypos:
    stx bFScanQuery
    lda _bFTeleportWordLength,x ; budget: 5+ letters 1, 9+ letters 2
    ldy #0
    cmp #5
    bcc @none
    iny
    cmp #9
    bcc @budget
    iny
@budget:
    sty bFEditBudget
    tay
    clc
    adc _bFTeleportWordStart,x
    sta bFEditEnd
    lda bFScanFlags,x
    and #SCAN_LAST
    sta bFEditPrefix
    tya                         ; lengths too far apart: no
    sec
    sbc bFScanLength            ; query - word
    bcc @longer
    cmp bFEditBudget
    beq @lengths
    bcs @none
    bra @lengths
@longer:
    ldy bFEditPrefix            ; a prefix may be shorter than the word
    bne @lengths
    eor #$FF
    inc a
    cmp bFEditBudget
    beq @lengths
    bcs @none
@lengths:
    ldy _bFTeleportWordStart,x
    lda (ptr2),y
    ldx #1
    cmp bFScanWord
    beq @level
    inx                         ; a wrong first letter counts two
@level:
    cpx bFEditBudget
    beq @try
    bcs @none
@try:
    stx bFEditLeft
    phx
    ldy bFScanQuery
    lda _bFTeleportWordStart,y
    tay
    ldx #0
    jsr bFEdit
    pla
    bcs @found
    tax
    inx
    bra @level
@found:
    ldx bFScanQuery
    rts
@none:
    ldx bFScanQuery
    clc
    rts

; Can query[Y..bFEditEnd) become word[X..bFScanLength) in bFEditLeft edits?
; A prefix may stop early. Carry set if so; bFEditLeft is kept.
bFEdit:
@same:
    cpy bFEditEnd
    beq @queryDone
    cpx bFScanLength
    beq @wordDone
    lda (ptr2),y
    cmp bFScanWord,x
    bne @differ
    iny
    inx
    bra @same
@queryDone:                     ; the word's rest: extra letters
    lda bFEditPrefix
    bne @yes
    txa
    eor #$FF
    sec
    adc bFScanLength
    bra @rest
@wordDone:                      ; the query's rest: missing letters
    tya
    eor #$FF
    sec
    adc bFEditEnd
@rest:
    sta bFEditRest
    lda bFEditLeft
    cmp bFEditRest
    rts
@yes:
    sec
    rts
@differ:
    lda bFEditLeft
    beq @no
    dec bFEditLeft
    phx
    phy
    inx                         ; a letter changed
    iny
    jsr bFEdit
    ply
    plx
    bcs @found
    phx
    phy
    iny                         ; the query's letter is missing
    jsr bFEdit
    ply
    plx
    bcs @found
    phx
    phy
    inx                         ; the word's letter is extra
    jsr bFEdit
    ply
    plx
    bcs @found
    phx
    phy
    inx                         ; two neighbours swapped
    cpx bFScanLength
    bcs @unswapped
    iny
    cpy bFEditEnd
    bcs @unswapped
    lda (ptr2),y
    cmp bFScanWord - 1,x
    bne @unswapped
    dey
    lda (ptr2),y
    cmp bFScanWord,x
    bne @unswapped
    iny
    iny
    inx
    jsr bFEdit
    .byte $24                   ; bit zp: skip the clc
@unswapped:
    clc
    ply
    plx
@found:
    inc bFEditLeft
    rts
@no:
    clc
    rts
.popcharmap
.endif
