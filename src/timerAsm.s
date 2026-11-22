.ifndef TIMER_INC
TIMER_INC = 1

.segment "CODE"
.export _timerEnabled
_timerEnabled: .byte $1          ; Global flag: non-zero = timer active, zero = disabled
.segment "BANKRAM06"

TIMER_TICK = 1                   ; Amount to advance the sub-second counter on each tick

SECONDS_VAR = 11                 ; AGI variable index for seconds
MINUTES_VAR = 12                 ; AGI variable index for minutes
DAYS_VAR = 13                    ; AGI variable index for days
HOURS_VAR = 14                   ; AGI variable index for hours

TIMER_VAR = IRQ_TMP_2            ; Temporary: holds the AGI variable index currently being updated
INC_NEXT_UNIT_AFTER = IRQ_TMP_2 + 1  ; Temporary: holds the rollover threshold for the current unit

; -----------------------------------------------------------------------------
; b6HandleTimeComponent
;
; Increments a single time unit stored in the AGI variable area and determines
; whether the next higher unit should also be incremented.
;
; Inputs:
;   TIMER_VAR           - index of the AGI variable to increment
;   INC_NEXT_UNIT_AFTER - value at which the unit rolls over (and next unit increments)
;
; Outputs:
;   A = 0 if no rollover occurred
;   A = 1 if rollover occurred (caller should advance the next unit)
;   The AGI variable is updated in place (via SET_VAR_OFFSET)
;
; Preserves: Y
; -----------------------------------------------------------------------------
b6HandleTimeComponent:
ldy TIMER_VAR
lda GOLDEN_RAM + VARS_AREA_START_GOLDEN_OFFSET,y   ; Load current value of the time unit
inc                                            ; Increment it
sta SET_VAR_OFFSET,y                           ; Store the new value back

cmp INC_NEXT_UNIT_AFTER                        ; Has this unit reached its rollover point?
bcs @timeHasPassed                             ; Yes → signal rollover
lda #$0                                        ; No → return 0 (no further units to advance)
rts

@timeHasPassed:
lda #$0
sta SET_VAR_OFFSET,y                           ; Reset this unit to 0
lda #$1                                        ; Return 1 (caller should advance next unit)
rts

; -----------------------------------------------------------------------------
; b6TimerTick
;
; C prototype equivalent:
;   void b6TimerTick(void);
;
; Called on each timer interrupt (or equivalent tick). Advances the internal
; hundredths counter and, on rollover, cascades through seconds → minutes →
; hours → days using AGI variables.
;
; The function is a no-op when the global _timerEnabled flag is clear.
;
; Side effects:
;   - Updates b6Hund (sub-second counter)
;   - May update AGI variables SECONDS_VAR, MINUTES_VAR, HOURS_VAR, DAYS_VAR
; -----------------------------------------------------------------------------
b6TimerTick:
lda _timerEnabled
beq @end                                       ; Timer disabled → exit immediately

clc
lda b6Hund
adc #TIMER_TICK                                ; Advance sub-second counter
sta b6Hund

cmp #60                                        ; Have we accumulated a full second?
bcs @clearHundreds                             ; Yes → cascade to higher units
rts                                            ; No → done for this tick

@clearHundreds:
stz b6Hund                                     ; Reset sub-second counter

@seconds:
lda #60
sta INC_NEXT_UNIT_AFTER                        ; Seconds roll over at 60
lda #SECONDS_VAR
sta TIMER_VAR
jsr b6HandleTimeComponent
beq @end                                       ; No minute rollover → done

@minutes:
lda #MINUTES_VAR
sta TIMER_VAR
jsr b6HandleTimeComponent                      ; Minutes also roll over at 60
beq @end                                       ; No hour rollover → done

@hours:
lda #HOURS_VAR
sta TIMER_VAR
lda #24
sta INC_NEXT_UNIT_AFTER                        ; Hours roll over at 24
jsr b6HandleTimeComponent
beq @end                                       ; No day rollover → done

@days:
lda #DAYS_VAR
sta TIMER_VAR
lda #255                                       ; No higher unit exists; effectively never rolls over
sta INC_NEXT_UNIT_AFTER
jsr b6HandleTimeComponent
beq @end

@end:
rts
b6Hund: .byte $0                               ; Sub-second accumulator (0–59)
.endif