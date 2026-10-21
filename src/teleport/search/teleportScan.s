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
.importzp ptr1, ptr2, ptr4, tmp1, tmp2
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
bFScanWord: .res TELEPORT_WORD_SIZE + TELEPORT_KEY_LENGTH ; length checked per key run
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
bFScanSkip: .res 1              ; bit 7: in a word no query word can match
bFScanKeyReversed: .res TELEPORT_KEY_LENGTH ; X counts down through the key
bFScanLetterNext: .res 1        ; the walk through both lists
bFScanLongNext: .res 1
SCAN_LAST = 1                   ; last word of its stop: also a prefix
SCAN_FIRST = 2                  ; first word of its stop
bFScanSaveY: .res 1

TELEPORT_ASCII
; Starts a room's message text; marks each stop's first and last word.
bFTeleportScanReset:
    stz bFScanSkip
    ldx #TELEPORT_KEY_LENGTH - 1
    stx bFScanKey
    ldy #0
@key:
    lda _avisDurgan,y
    sta bFScanKeyReversed,x
    iny
    dex
    bpl @key
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

; Scans A bytes (1-255) at ptr1 (low RAM); ptr1[A] must be writable. A byte
; with bit 7 set there decrypts to no character a word or a space can be,
; so the only test per byte is the key's: X counts down through it. Three
; loops: between words; in a word a query word can match, its letters
; stored at (ptr4),y; and in a word none can, only looked through for its
; end. Decryption and word state carry over to the next call.
bFTeleportScan:
    sta tmp1
    tay
    lda #$80                    ; the end mark
    sta (ptr1),y
    jsr bFTeleportLinePointer2
    ldx bFScanKey
    ldy #0
    bit bFScanSkip
    bmi @skip
    lda bFScanLength
    bne @word
    jmp @gap
@word:
    cmp #TELEPORT_WORD_SIZE
    bcs @skip                   ; too long to match
    adc #<bFScanWord            ; carry clear: (ptr4),y is the next letter
    sta ptr4
    lda #>bFScanWord
    adc #0
    sta ptr4 + 1
    jmp @store
@skipKey:
    ldx #TELEPORT_KEY_LENGTH - 1
@skip:
    lda (ptr1),y
    eor bFScanKeyReversed,x
    cmp #'a'
    bcc @skipOther
    cmp #'z' + 1
    bcs @skipEnd
@skipNext:
    iny
    dex
    bpl @skip
    bra @skipKey
@skipOther:
    cmp #' '                    ; the most common end
    beq @skipSpace
    cmp #'A'
    bcc @skipDigit
    cmp #'Z' + 1
    bcc @skipNext
    bra @skipEnd
@skipDigit:
    cmp #'0'
    bcc @skipApostrophe
    cmp #'9' + 1
    bcc @skipNext
    bra @skipEnd
@skipApostrophe:
    cmp #$27
    beq @skipNext
@skipEnd:
    cpy tmp1
    beq @skipped                ; the end mark: the word goes on
    inc bFScanPosition
    bne @mark
    inc bFScanPosition + 1
    bra @mark
@skipSpace:
    inc bFScanPosition
    bne @gapNext
    inc bFScanPosition + 1
    bra @gapNext
@skipped:
    lda #$80
    sta bFScanSkip
    stz bFScanLength
    stx bFScanKey
    rts
@mark:                          ; A ends no word, or one just ended
    cmp #' '                    ; the most common
    beq @gapNext
    cmp #0                      ; a sentence or message ends: far apart
    beq @sentence
    cmp #'.'
    beq @sentence
    cmp #'!'
    beq @sentence
    cmp #'?'
    beq @sentence
    cpy tmp1
    bne @gapNext
    stz bFScanSkip              ; the end mark
    stz bFScanLength
    stx bFScanKey
    rts
@sentence:
    lda bFScanPosition
    clc
    adc #TELEPORT_SENTENCE
    sta bFScanPosition
    bcc @gapNext
    inc bFScanPosition + 1
@gapNext:
    iny
    dex
    bpl @gap
@gapKey:
    ldx #TELEPORT_KEY_LENGTH - 1
@gap:
    lda (ptr1),y
    eor bFScanKeyReversed,x
    cmp #'a'
    bcc @gapOther
    cmp #'z' + 1
    bcs @mark
@first:                         ; a word starts: can a query word match it?
    phx
    tax
    lda bFScanStarts + 10 - 'a',x
@listed:
    ora bFScanLong
    beq @unlisted
    txa
    plx
    sta bFScanWord
    sty ptr4                    ; ptr4 = bFScanWord - Y
    lda #<bFScanWord
    sec
    sbc ptr4
    sta ptr4
    lda #>bFScanWord
    sbc #0
    sta ptr4 + 1
    jmp @storeNext
@unlisted:
    plx
    jmp @skipNext
@gapOther:
    cmp #'A'
    bcc @gapDigit
    cmp #'Z' + 1
    bcs @mark
    ora #$20
    bra @first
@gapDigit:
    cmp #'0'
    bcc @gapApostrophe
    cmp #'9' + 1
    bcs @mark
    phx
    tax
    lda bFScanStarts - '0',x
    bra @listed
@gapApostrophe:
    cmp #$27                    ; dropped
    beq @dropped
    jmp @mark
@dropped:
    jmp @gapNext
@storeKey:                      ; and too long to match yet?
    ldx #TELEPORT_KEY_LENGTH - 1
    tya
    clc
    adc ptr4
    sec
    sbc #<bFScanWord
    cmp #TELEPORT_WORD_SIZE
    bcc @store
    jmp @skip
@store:
    lda (ptr1),y
    eor bFScanKeyReversed,x
    cmp #'a'
    bcc @storeOther
    cmp #'z' + 1
    bcs @storeEnd
@put:
    sta (ptr4),y
@storeNext:
    iny
    dex
    bpl @store
    bra @storeKey
@storeOther:
    cmp #'A'
    bcc @storeDigit
    cmp #'Z' + 1
    bcs @storeEnd
    ora #$20
    bra @put
@storeDigit:
    cmp #'0'
    bcc @storeApostrophe
    cmp #'9' + 1
    bcc @put
    bra @storeEnd
@storeApostrophe:
    cmp #$27
    bne @storeEnd
    lda ptr4                    ; dropped: the next letter takes its place
    bne @back
    dec ptr4 + 1
@back:
    dec ptr4
    bra @storeNext
@storeEnd:
    pha
    tya                         ; the word's length
    clc
    adc ptr4
    sec
    sbc #<bFScanWord
    sta bFScanLength
    pla
    cpy tmp1
    beq @stored                 ; the end mark: the word goes on
    pha
    stx bFScanKey
    jsr bFScanEndWord
    ldx bFScanKey
    pla
    jmp @mark
@stored:
    stz bFScanSkip
    stx bFScanKey
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
    bcs @clear
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
