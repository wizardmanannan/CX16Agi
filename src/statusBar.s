.include "x16.inc"
.ifndef STATUSBAR_INC
STATUSBAR_INC = 1

.segment "CODE"

_statusLineDisplayed: .byte $0

STATUS_BAR_LOCATION = $DA00
STATUS_BAR_TILE = $10
SCORE_TO_SOUND_PADDING = 20

.segment "ZEROPAGE"
TEXT_TO_PAD_MAX_SCORE: .byte $0
LAST_PRINTED_SCORE: .byte $0
.segment "BANKRAM10"
score: .byte $53, $63, $6F, $72, $65, $3A, $00 ;Score
of: .byte $6F, $66, $00 ;of
sound: .byte $53, $6F, $75, $6E, $64, $3A, $00; Sound
on: .byte $4F, $6E, $20, $00
off: .byte $4F, $66, $66, $00
totalScore: .res 3
totalScoreSet: .byte $0

; ===================================================================
; Units digit (always ASCII)
; ===================================================================
b10ScoreUnits:
    .byte 48,49,50,51,52,53,54,55,56,57   ; 0-9
    .byte 48,49,50,51,52,53,54,55,56,57   ; 10-19
    .byte 48,49,50,51,52,53,54,55,56,57   ; 20-29
    .byte 48,49,50,51,52,53,54,55,56,57   ; 30-39
    .byte 48,49,50,51,52,53,54,55,56,57   ; 40-49
    .byte 48,49,50,51,52,53,54,55,56,57   ; 50-59
    .byte 48,49,50,51,52,53,54,55,56,57   ; 60-69
    .byte 48,49,50,51,52,53,54,55,56,57   ; 70-79
    .byte 48,49,50,51,52,53,54,55,56,57   ; 80-89
    .byte 48,49,50,51,52,53,54,55,56,57   ; 90-99
    .byte 48,49,50,51,52,53,54,55,56,57   ; 100-109
    .byte 48,49,50,51,52,53,54,55,56,57   ; 110-119
    .byte 48,49,50,51,52,53,54,55,56,57   ; 120-129
    .byte 48,49,50,51,52,53,54,55,56,57   ; 130-139
    .byte 48,49,50,51,52,53,54,55,56,57   ; 140-149
    .byte 48,49,50,51,52,53,54,55,56,57   ; 150-159
    .byte 48,49,50,51,52,53,54,55,56,57   ; 160-169
    .byte 48,49,50,51,52,53,54,55,56,57   ; 170-179
    .byte 48,49,50,51,52,53,54,55,56,57   ; 180-189
    .byte 48,49,50,51,52,53,54,55,56,57   ; 190-199
    .byte 48,49,50,51,52,53,54,55,56,57   ; 200-209
    .byte 48,49,50,51,52,53,54,55,56,57   ; 210-219
    .byte 48,49,50,51,52,53,54,55,56,57   ; 220-229
    .byte 48,49,50,51,52,53,54,55,56,57   ; 230-239
    .byte 48,49,50,51,52,53,54,55,56,57   ; 240-249
    .byte 48,49,50,51,52,53,54,55,56,57   ; 250-255

; ===================================================================
; Tens digit
; ===================================================================
b10ScoreTens:
    .byte 0,0,0,0,0,0,0,0,0,0, 48,48,48,48,48,48,48,48,48,48  ; 0-19
    .byte 49,49,49,49,49,49,49,49,49,49, 50,50,50,50,50,50,50,50,50,50  ; 20-39
    .byte 51,51,51,51,51,51,51,51,51,51, 52,52,52,52,52,52,52,52,52,52  ; 40-59
    .byte 53,53,53,53,53,53,53,53,53,53, 54,54,54,54,54,54,54,54,54,54  ; 60-79
    .byte 55,55,55,55,55,55,55,55,55,55, 56,56,56,56,56,56,56,56,56,56  ; 80-99
    .byte 48,48,48,48,48,48,48,48,48,48, 49,49,49,49,49,49,49,49,49,49  ; 100-119
    .byte 50,50,50,50,50,50,50,50,50,50, 51,51,51,51,51,51,51,51,51,51  ; 120-139
    .byte 52,52,52,52,52,52,52,52,52,52, 53,53,53,53,53,53,53,53,53,53  ; 140-159   ← 158 is here (53 = '5')
    .byte 54,54,54,54,54,54,54,54,54,54, 55,55,55,55,55,55,55,55,55,55  ; 160-179
    .byte 56,56,56,56,56,56,56,56,56,56, 57,57,57,57,57,57,57,57,57,57  ; 180-199
    .byte 48,48,48,48,48,48,48,48,48,48, 49,49,49,49,49,49,49,49,49,49  ; 200-219
    .byte 50,50,50,50,50,50,50,50,50,50, 51,51,51,51,51,51,51,51,51,51  ; 220-239
    .byte 52,52,52,52,52,52,52,52,52,52, 53,53,53,53,53,53               ; 240-255

; ===================================================================
; Hundreds digit
; ===================================================================
b10ScoreHundreds:
    ; 0-99
    .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    ; 100-199
    .byte 49,49,49,49,49,49,49,49,49,49   ; 100-109
    .byte 49,49,49,49,49,49,49,49,49,49   ; 110-119
    .byte 49,49,49,49,49,49,49,49,49,49   ; 120-129
    .byte 49,49,49,49,49,49,49,49,49,49   ; 130-139
    .byte 49,49,49,49,49,49,49,49,49,49   ; 140-149
    .byte 49,49,49,49,49,49,49,49,49,49   ; 150-159   ← 158 = 49 ('1')
    .byte 49,49,49,49,49,49,49,49,49,49   ; 160-169
    .byte 49,49,49,49,49,49,49,49,49,49   ; 170-179
    .byte 49,49,49,49,49,49,49,49,49,49   ; 180-189
    .byte 49,49,49,49,49,49,49,49,49,49   ; 190-199
    ; 200-255
    .byte 50,50,50,50,50,50,50,50,50,50   ; 200-209
    .byte 50,50,50,50,50,50,50,50,50,50   ; 210-219
    .byte 50,50,50,50,50,50,50,50,50,50   ; 220-229
    .byte 50,50,50,50,50,50,50,50,50,50   ; 230-239
    .byte 50,50,50,50,50,50,50,50,50,50   ; 240-249
    .byte 50,50,50,50,50,50               ; 250-255

TEXT_TO_PRINT = IRQ_TMP_1
TEXT_TO_PAD = IRQ_TMP_2 + 1
STATUS_BAR_SREG = IRQ_TMP_3

b10StatusBarShowing: .byte $0

b10PrintStatusBarString:
ldy #$0
@printTextLoop:
lda (TEXT_TO_PRINT),y
beq @return
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0
iny
bra @printTextLoop

@return:
rts

b10DisplayStatusBar:
jsr isMenuAllowed
and _menuShown
beq @checkStatusBarShown ;If the menu is displayed don't display the status bar
stz b10StatusBarShowing
rts
@checkStatusBarShown:
lda b10StatusBarShowing ;if(!b10StatusBarShowing && _statusBarDisplay)
tax
eor #$1
and _statusLineDisplayed
bne @displayStatusBar
txa
beq @notShowingNotDisplay
@clearStatusBar:
lda _statusLineDisplayed
bne @isShowingShouldDisplay
@isShowingShouldNotDisplay:
JSRFAR bFClearTopLine,MENU_BANK
@notShowingNotDisplay:
rts
@isShowingShouldDisplay:
ldy #SCORE_VAR
GET_VAR_NON_INTERPRETER
cmp LAST_PRINTED_SCORE
bne @displayStatusBar

rts

@displayStatusBar:
lda #$1 
sta b10StatusBarShowing
stz VERA_ctrl
lda #<STATUS_BAR_LOCATION
sta VERA_addr_low
lda #>STATUS_BAR_LOCATION
sta VERA_addr_high
lda #$10 
sta VERA_addr_bank

lda #SPACE
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

lda #<score
sta TEXT_TO_PRINT
lda #>score
sta TEXT_TO_PRINT + 1
jsr b10PrintStatusBarString

@printScore:
lda totalScoreSet
bne @loadTextToPad

stz b10StatusBarShowing

lda totalScoreSet
bne @loadTextToPad
jmp @setTotalScore

@loadTextToPad:
lda TEXT_TO_PAD_MAX_SCORE
sta TEXT_TO_PAD

@printScoreText:
ldy #SCORE_VAR
GET_VAR_NON_INTERPRETER
tay
sty LAST_PRINTED_SCORE

lda b10ScoreHundreds,y
beq @printScoreTens
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printScoreTens:
lda b10ScoreTens,y
beq @printScoreUnits
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printScoreUnits:
lda b10ScoreUnits,y
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printOf:
lda #SPACE
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

lda #<of
sta TEXT_TO_PRINT
lda #>of
sta TEXT_TO_PRINT + 1
jsr b10PrintStatusBarString

@printTotalScore:
lda #SPACE
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printStoredTotalScoreValue:

@printTotalScoreHundreds:
lda totalScore + 2
beq @printTotalScoreTens
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printTotalScoreTens:
lda totalScore + 1
beq @printTotalScoreUnits
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printTotalScoreUnits:
lda totalScore
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

ldy TEXT_TO_PAD
beq @printSound
@printPaddingLoop:
lda #SPACE
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0
dey
bne @printPaddingLoop

@printSound:
lda #<sound
sta TEXT_TO_PRINT
lda #>sound
sta TEXT_TO_PRINT + 1
jsr b10PrintStatusBarString

ldy #SOUND_VAR 
GET_FLAG_NON_INTERPRETER STATUS_BAR_SREG
bne @soundOn
@soundOff:
lda #<off
sta TEXT_TO_PRINT
lda #>off
sta TEXT_TO_PRINT + 1
bra @printSoundStatus
@soundOn:
lda #<on
sta TEXT_TO_PRINT
lda #>on
sta TEXT_TO_PRINT + 1

@printSoundStatus:
jsr b10PrintStatusBarString

rts
@setTotalScore:
lda #SCORE_TO_SOUND_PADDING
sta TEXT_TO_PAD_MAX_SCORE


ldy #MAX_SCORE_VAR
GET_VAR_NON_INTERPRETER
beq @printSound

tay
lda b10ScoreUnits,y
sta totalScore
dec TEXT_TO_PAD_MAX_SCORE

lda b10ScoreTens,y
sta totalScore + 1
beq @setTotalScoreHundreds
dec TEXT_TO_PAD_MAX_SCORE

@setTotalScoreHundreds:
lda b10ScoreHundreds,y
sta totalScore + 2
dec TEXT_TO_PAD_MAX_SCORE

@incrementTotalScore:
inc totalScoreSet
jmp @loadTextToPad

.endif