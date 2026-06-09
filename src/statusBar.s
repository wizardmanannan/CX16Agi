.include "x16.inc"

.ifndef STATUSBAR_INC
STATUSBAR_INC = 1

.segment "CODE"

_statusLineDisplayed: .byte $0          ; Global flag: is the status line currently shown?

; ===================================================================
; Status Bar Constants
; ===================================================================
STATUS_BAR_LOCATION = $DA00             ; VRAM address for the status bar (same row as menu bar)
STATUS_BAR_TILE = $10                   ; Palette attribute byte used for status bar text
SCORE_TO_SOUND_PADDING = 20             ; Padding spaces between score and sound sections

.segment "ZEROPAGE"
TEXT_TO_PAD_MAX_SCORE: .byte $0         ; Maximum padding needed for score display
LAST_PRINTED_SCORE: .byte $0            ; Last score value printed (used to avoid unnecessary redraws)
.segment "BANKRAM10"

; ===================================================================
; Status Bar Text Strings (ASCII)
; ===================================================================
score: .byte $53, $63, $6F, $72, $65, $3A, $00 ; "Score:"
of: .byte $6F, $66, $00                         ; "of"
sound: .byte $53, $6F, $75, $6E, $64, $3A, $00 ; "Sound:"
on: .byte $4F, $6E, $20, $00                    ; "On "
off: .byte $4F, $66, $66, $00                   ; "Off"

totalScore: .res 3                              ; 3-byte storage for total/max score display (hundreds, tens, units)
totalScoreSet: .byte $0                         ; Flag: has totalScore been initialized?

; ===================================================================
; b10ScoreUnits - Units digit lookup table (always ASCII)
; ===================================================================
; This table provides the ASCII character for the units digit (0-9)
; for every possible score value 0-255.
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
; b10ScoreTens - Tens digit lookup table (ASCII)
; ===================================================================
b10ScoreTens:
    .byte 0,0,0,0,0,0,0,0,0,0, 48,48,48,48,48,48,48,48,48,48  ; 0-19
    .byte 49,49,49,49,49,49,49,49,49,49, 50,50,50,50,50,50,50,50,50,50  ; 20-39
    .byte 51,51,51,51,51,51,51,51,51,51, 52,52,52,52,52,52,52,52,52,52  ; 40-59
    .byte 53,53,53,53,53,53,53,53,53,53, 54,54,54,54,54,54,54,54,54,54  ; 60-79
    .byte 55,55,55,55,55,55,55,55,55,55, 56,56,56,56,56,56,56,56,56,56  ; 80-99
    .byte 48,48,48,48,48,48,48,48,48,48, 49,49,49,49,49,49,49,49,49,49  ; 100-119
    .byte 50,50,50,50,50,50,50,50,50,50, 51,51,51,51,51,51,51,51,51,51  ; 120-139
    .byte 52,52,52,52,52,52,52,52,52,52, 53,53,53,53,53,53,53,53,53,53  ; 140-159
    .byte 54,54,54,54,54,54,54,54,54,54, 55,55,55,55,55,55,55,55,55,55  ; 160-179
    .byte 56,56,56,56,56,56,56,56,56,56, 57,57,57,57,57,57,57,57,57,57  ; 180-199
    .byte 48,48,48,48,48,48,48,48,48,48, 49,49,49,49,49,49,49,49,49,49  ; 200-219
    .byte 50,50,50,50,50,50,50,50,50,50, 51,51,51,51,51,51,51,51,51,51  ; 220-239
    .byte 52,52,52,52,52,52,52,52,52,52, 53,53,53,53,53,53               ; 240-255

; ===================================================================
; b10ScoreHundreds - Hundreds digit lookup table (ASCII)
; ===================================================================
b10ScoreHundreds:
    ; 0-99 (no hundreds digit)
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
    .byte 49,49,49,49,49,49,49,49,49,49   ; 150-159
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


TEXT_TO_PRINT = IRQ_TMP_1               ; ZP pointer to current text string
TEXT_TO_PAD = IRQ_TMP_2 + 1             ; Remaining padding spaces to print
STATUS_BAR_SREG = IRQ_TMP_3             ; Temporary register for flag checks

b10StatusBarShowing: .byte $0           ; Flag: is status bar currently displayed?

; ================================================================
; b10PrintStatusBarString
; ================================================================
; Purpose: Prints a null-terminated string to the status bar in VRAM.
;          Each character is followed by the STATUS_BAR_TILE attribute.
; Input:   TEXT_TO_PRINT = pointer to null-terminated string
; Output:  Text written to current VERA address
; ================================================================
b10PrintStatusBarString:
ldy #$0
@printTextLoop:
lda (TEXT_TO_PRINT),y                   ; Read next character
beq @return                             ; Null terminator → done
sta VERA_data0                          ; Write character tile
lda #STATUS_BAR_TILE                    ; Load status bar palette/attribute
sta VERA_data0                          ; Write attribute byte
iny
bra @printTextLoop

@return:
rts


; ================================================================
; b10DisplayStatusBar
; ================================================================
; Purpose: Main routine to display or update the status bar.
;          Skips drawing if menu is active. Handles score changes
;          and total score initialization.
;
; C-like logic structure:
; if (menuAllowed && menuShown) {
;     hide status bar;
;     return;
; }
; if (!b10StatusBarShowing && _statusLineDisplayed) {
;     display status bar;
; } else if (b10StatusBarShowing && !_statusLineDisplayed) {
;     clear status bar;
; } else if (score != LAST_PRINTED_SCORE) {
;     display status bar;
; }
;
; Input:   Various global state variables (score, sound flag, etc.)
; Output:  Updates VRAM at STATUS_BAR_LOCATION
; ================================================================
b10DisplayStatusBar:
jsr isMenuAllowed                       ; Check if menu system is allowed
and _menuShown                          ; If menu is shown, don't draw status bar
beq @checkStatusBarShown                ; if (!(menuAllowed && menuShown))
stz b10StatusBarShowing
rts

@checkStatusBarShown:
lda b10StatusBarShowing                 ; Check current status bar visibility
tax
eor #$1                                 ; Toggle logic for comparison
and _statusLineDisplayed
bne @displayStatusBar                   ; if (!b10StatusBarShowing && _statusLineDisplayed)

txa
beq @notShowingNotDisplay

@clearStatusBar:
lda _statusLineDisplayed
bne @isShowingShouldDisplay             ; if (b10StatusBarShowing && !_statusLineDisplayed)

@isShowingShouldNotDisplay:
JSRFAR bAClearTopLine, MENU_BANK        ; Clear the top line via menu bank routine
stz b10StatusBarShowing

@notShowingNotDisplay:
rts

@isShowingShouldDisplay:
ldy #SCORE_VAR
GET_VAR_NON_INTERPRETER                 ; Get current score
cmp LAST_PRINTED_SCORE                  ; Has score changed?
bne @displayStatusBar                   ; if (score != LAST_PRINTED_SCORE)
rts                                     ; Score unchanged → no need to redraw

@displayStatusBar:
lda #$1 
sta b10StatusBarShowing                 ; Mark status bar as visible

stz VERA_ctrl
lda #<STATUS_BAR_LOCATION               ; Set VRAM address to status bar start
sta VERA_addr_low
lda #>STATUS_BAR_LOCATION
sta VERA_addr_high
lda #$10                                ; Bank 0 + increment by 1
sta VERA_addr_bank

lda #SPACE                              ; Leading space
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

lda #<score                             ; Print "Score:"
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
jmp @setTotalScore                      ; First time - initialize total score display

@loadTextToPad:
lda TEXT_TO_PAD_MAX_SCORE
sta TEXT_TO_PAD

@printScoreText:
ldy #SCORE_VAR
GET_VAR_NON_INTERPRETER
tay
sty LAST_PRINTED_SCORE                  ; Remember last printed score

lda b10ScoreHundreds,y                  ; Print hundreds digit if needed
beq @printScoreTens
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printScoreTens:
lda b10ScoreTens,y                      ; Print tens digit if needed
beq @printScoreUnits
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printScoreUnits:
lda b10ScoreUnits,y                     ; Always print units digit
dec TEXT_TO_PAD
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printOf:
lda #SPACE                              ; Space before "of"
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
lda totalScore + 2                      ; Hundreds digit of total/max score
beq @printTotalScoreTens
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printTotalScoreTens:
lda totalScore + 1                      ; Tens digit of total score
beq @printTotalScoreUnits
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

@printTotalScoreUnits:
lda totalScore                          ; Units digit of total score
sta VERA_data0
lda #STATUS_BAR_TILE
sta VERA_data0

ldy TEXT_TO_PAD                         ; Print remaining padding spaces
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
lda b10ScoreUnits,y                     ; Store units digit of max score
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
inc totalScoreSet                       ; Mark total score as initialized
jmp @loadTextToPad

.endif