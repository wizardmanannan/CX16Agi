.ifndef TELEPORT_GRAMMAR_INC
TELEPORT_GRAMMAR_INC = 1
; teleportGrammar.s - What did the user ask for?
;
;   teleport DEST                     tp DEST
;   teleport:[SECONDS:]DEST,DEST...   tp:[SECONDS:]DEST,DEST...
;   DEST = ROOM [X Y] | NAME WORDS... [X Y]
;
; One pass over the line, with a three-number look-behind instead of a token
; list. Name words are kept as offsets into the line, not copied. X and Y are
; range-checked here; rooms, names and cancellation forms are judged by the
; command (teleportCommand.s).
.importzp ptr1, tmp1, tmp2, tmp3
.export _bFTeleportStopCount, _bFTeleportStopRoom, _bFTeleportStopX
.export _bFTeleportStopY, _bFTeleportStopFirst, _bFTeleportWords
.export _bFTeleportWordStart, _bFTeleportWordLength, _bFTeleportDelay
.export _bFTeleportTourForm

TELEPORT_MAX_STOPS = 8
TELEPORT_MAX_WORDS = 12
TELEPORT_WORD_SIZE = 16         ; longest word + 1
TELEPORT_DEFAULT_DELAY = 5
TOKEN_DIGITS = $01              ; token kinds, as bits in tmp3
TOKEN_LETTERS = $02
TOKEN_INVALID = $80

.segment "BANKRAM0F"
_bFTeleportStopCount: .res 1
_bFTeleportStopRoom: .res TELEPORT_MAX_STOPS     ; 0 for names until found
_bFTeleportStopX: .res TELEPORT_MAX_STOPS        ; TELEPORT_AUTO if not given
_bFTeleportStopY: .res TELEPORT_MAX_STOPS
_bFTeleportStopFirst: .res TELEPORT_MAX_STOPS + 1 ; first word of each stop
_bFTeleportWords: .res 1
_bFTeleportWordStart: .res TELEPORT_MAX_WORDS    ; offsets into the line
_bFTeleportWordLength: .res TELEPORT_MAX_WORDS
_bFTeleportDelay: .res 1                         ; seconds per tour stop
_bFTeleportTourForm: .res 1                      ; nonzero for "teleport:"
bFGrammarNumbers: .res 3
bFGrammarCount: .res 1
bFTokenStart: .res 1
bFTokenLength: .res 1

TELEPORT_ASCII
; In: ptr1 = line. Out: carry clear if well formed.
bFTeleportGrammar:
    ldy #1
    lda (ptr1),y
    ldy #8
    cmp #'p'
    bne @keyword
    ldy #2
@keyword:
    stz _bFTeleportStopCount
    stz _bFTeleportWords
    stz _bFTeleportStopFirst
    stz _bFTeleportTourForm
    lda #TELEPORT_DEFAULT_DELAY
    sta _bFTeleportDelay
    lda (ptr1),y
    cmp #':'
    bne @stop
    sta _bFTeleportTourForm
    iny
    phy                         ; SECONDS: is optional
    jsr bFToken
    lda (ptr1),y
    cmp #':'
    bne @noDelay
    lda tmp3
    cmp #TOKEN_DIGITS
    bne @noDelay
    lda tmp2
    sta _bFTeleportDelay
    iny
    plx
    bra @stop
@noDelay:
    ply
@stop:
    jsr bFTeleportStop
    bcs @done
    lda (ptr1),y
    beq @done                   ; carry is clear
    lda _bFTeleportTourForm     ; ',' separates tour stops only
    beq @error
    iny
    bra @stop
@error:
    sec
@done:
    rts

; Parses one stop from Y to ',' or the end. Out: carry set on error.
bFTeleportStop:
    lda _bFTeleportStopCount
    cmp #TELEPORT_MAX_STOPS
    bcs @failed
    stz bFGrammarCount
@token:
    lda (ptr1),y
    cmp #' '
    bne @scan
    iny
    bra @token
@scan:
    jsr bFToken
    beq @end
    bmi @failed
    and #TOKEN_LETTERS
    bne @name
    ldx bFGrammarCount          ; a number: room, X or Y
    cpx #3
    bcs @failed
    lda tmp2
    sta bFGrammarNumbers,x
    inc bFGrammarCount
    bra @token
@name:
    lda bFGrammarCount          ; numbers only after the whole name
    bne @failed
    ldx _bFTeleportWords
    cpx #TELEPORT_MAX_WORDS
    bcs @failed
    lda bFTokenLength
    cmp #TELEPORT_WORD_SIZE
    bcs @failed
    sta _bFTeleportWordLength,x
    lda bFTokenStart
    sta _bFTeleportWordStart,x
    inc _bFTeleportWords
    bra @token
@failed:
    sec
    rts
@end:
    lda (ptr1),y
    cmp #':'
    beq @failed
    phy
    ldx _bFTeleportStopCount
    lda #TELEPORT_AUTO
    sta _bFTeleportStopX,x
    stz _bFTeleportStopRoom,x
    lda _bFTeleportWords
    sta _bFTeleportStopFirst + 1,x
    sec
    sbc _bFTeleportStopFirst,x
    beq @numeric
    ldy #0                      ; NAME [X Y]
    lda bFGrammarCount
    beq @accept
    cmp #2
    beq @coordinates
    bra @rejected
@numeric:
    lda bFGrammarNumbers        ; ROOM [X Y]
    sta _bFTeleportStopRoom,x
    ldy #1
    lda bFGrammarCount
    dec a
    beq @accept
    cmp #2
    bne @rejected
@coordinates:
    lda bFGrammarNumbers,y
    cmp #TELEPORT_WIDTH
    bcs @rejected
    sta _bFTeleportStopX,x
    lda bFGrammarNumbers + 1,y
    cmp #TELEPORT_HEIGHT
    bcs @rejected
    sta _bFTeleportStopY,x
@accept:
    inc _bFTeleportStopCount
    ply
    clc
    rts
@rejected:
    ply
    sec
    rts

; Scans the token at Y up to a space, ',', ':' or the end.
; Out: A = tmp3 = TOKEN_* bits (0: empty), N set if invalid; tmp2 = value of
; an all-digit token (0-255, larger is invalid); bFTokenStart/Length.
bFToken:
    sty bFTokenStart
    stz tmp2
    stz tmp3
@char:
    lda (ptr1),y
    beq @end
    cmp #' '
    beq @end
    cmp #','
    beq @end
    cmp #':'
    beq @end
    cmp #'a'
    bcc @digit
    cmp #'z' + 1
    bcs @invalid
    lda #TOKEN_LETTERS
    bra @mark
@digit:
    cmp #'0'
    bcc @invalid
    cmp #'9' + 1
    bcs @invalid
    and #$0F
    sta tmp1
    lda tmp2                    ; value * 10 + digit, at most 255
    cmp #26
    bcs @invalid
    asl a
    asl a
    adc tmp2
    asl a
    adc tmp1
    bcs @invalid
    sta tmp2
    lda #TOKEN_DIGITS
    .byte $2C                   ; bit abs: skip the lda
@invalid:
    lda #TOKEN_INVALID
@mark:
    tsb tmp3
    iny
    bra @char
@end:
    tya
    sec
    sbc bFTokenStart
    sta bFTokenLength
    lda tmp3
    rts
.popcharmap
.endif
