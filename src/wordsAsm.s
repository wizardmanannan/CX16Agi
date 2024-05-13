.segment "BANKRAM12"
.import _numWords
.import _wordsTextStart
.import _memcpyBanked
.import _b12CompareWithWordNumber

.macro COMPARE_BYTE_POSSIBLE_TERMINATOR BYTE_EQUAL_LABEL, INDEX
.local @checkByteForTerminators
.local @checkByteNonTerminated
.local @checkByteTerminator
ldy INDEX

@checkByteForTerminators:
lda (WORDS_TEXT_START),y
tax  ;Store (WORDS_TEXT_START),y so we can bring it back to a when we check in the terminator
beq @checkByteTerminator
lda USER_WORD_FIX_THREE_BYTES,y
beq @checkByteTerminator

@checkByteNonTerminated:
stx WORDS_TEXT_START_CACHE
cmp WORDS_TEXT_START_CACHE
beq BYTE_EQUAL_LABEL
bcc @less
bra @greater

@checkByteTerminator:
txa
cmp USER_WORD_FIX_THREE_BYTES,y
beq @prefixEqual ; They both have a terminator and the word must be equal. Even though we know the words must be equal, we still jump to prefix equal to get the prefix
txa ;For the purpose of figuring out if (WORDS_TEXT_START),y
beq @greater ;This means (WORDS_TEXT_START),y is terminated and the other is not
bra @less ;This means USER_WORD_FIX_THREE_BYTES,y
.endmacro

USER_WORD_ADDR = ZP_TMP_2
TOP = ZP_TMP_3
BOTTOM = ZP_TMP_4
MID = ZP_TMP_5
TMP = ZP_TMP_6
WORDS_TEXT_START = ZP_TMP_7
USER_WORD_FIX_THREE_BYTES = ZP_TMP_8 ;Takes three bytes
SYN = ZP_TMP_10
WORDS_TEXT_START_CACHE = ZP_TMP_12

_b12FindSynonymNumSearch:
sta USER_WORD_ADDR
stx USER_WORD_ADDR + 1

lda (USER_WORD_ADDR)
sta USER_WORD_FIX_THREE_BYTES
ldy #$1
lda (USER_WORD_ADDR),y
sta USER_WORD_FIX_THREE_BYTES + 1
ldy #$2
lda (USER_WORD_ADDR),y
sta USER_WORD_FIX_THREE_BYTES + 2

sec ;Top = numWords - 1
lda _numWords
sbc #$1
sta TOP
lda _numWords + 1
sbc #$0
sta TOP + 1

stz BOTTOM ;Bottom = 0
stz BOTTOM + 1

@while: ;while ((!found) && (bottom <= top))
LESS_THAN_OR_EQ_16 BOTTOM, TOP, @loopBody, @notFound
@notFound:
lda #$FF
ldx #$FF
rts

@loopBody:
clc
lda TOP ;mid = (top + bottom) / 2;
adc BOTTOM ;Plus Part
sta MID
lda TOP + 1
adc BOTTOM + 1
sta MID + 1

clc ;Divide Part Already have mid + 1 loaded
lsr
sta MID + 1
lda MID
ror
sta MID

;Multiple mid by 3 to get right offset into words metadata. 
clc ;Mult by 2 part
lda MID 
asl
sta TMP
lda MID + 1
rol 
sta TMP + 1
clc ; + MID Part
lda MID
adc TMP
sta TMP
lda MID + 1
adc TMP + 1
sta TMP + 1

clc
lda TMP
adc #<_wordsTextStart
sta WORDS_TEXT_START
lda TMP + 1
adc #>_wordsTextStart
sta WORDS_TEXT_START + 1

@checkByte1:
lda USER_WORD_FIX_THREE_BYTES 
cmp (WORDS_TEXT_START)
beq @checkByte2
bcc @less
bra @greater

@checkByte2:
COMPARE_BYTE_POSSIBLE_TERMINATOR @checkByte3, #$1

@checkByte3:
COMPARE_BYTE_POSSIBLE_TERMINATOR @prefixEqual, #$2

@less:
lda MID 
beq @lessTwoBytes
@lessOneByte:
dec
sta TOP
lda MID + 1
sta TOP + 1
jmp @while
@lessTwoBytes:
dec
sta TOP
lda MID + 1
dec
sta TOP + 1
jmp @while

@greater:
lda MID + 1 ;Putting these here will save cycles when we don't increment the high byte, because we can skip the high byte altogether by branching after inc. It will cost some cycles when we actually need to increment the high byte because these instructions will be pointless
sta BOTTOM + 1
lda MID
inc
sta BOTTOM
beq @greaterHigh
jmp @while

@greaterHigh:
lda MID + 1
inc
sta BOTTOM + 1
jmp @while

@prefixEqual:
lda MID 
ldx MID + 1
jsr pushax
lda USER_WORD_ADDR
ldx USER_WORD_ADDR + 1
jsr pushax
lda #<SYN
ldx #>SYN
jsr _b12CompareWithWordNumber
cmp #$0
beq @foundWord
bcc @less
bra @greater

jmp @while
@loopEnd:

@foundWord:
lda SYN
ldx SYN + 1

rts