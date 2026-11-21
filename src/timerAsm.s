.ifndef TIMER_INC
TIMER_INC = 1

.segment "BANKRAM06"

TIMER_TICK = 1

SECONDS_VAR = 11
MINUTES_VAR = 12
DAYS_VAR = 13
HOURS_VAR = 14

TIMER_VAR = IRQ_TMP_2
INC_NEXT_UNIT_AFTER = IRQ_TMP_2 + 1

b6HandleTimeComponent:
ldy TIMER_VAR
lda GOLDEN_RAM + VARS_AREA_START_GOLDEN_OFFSET,y   
inc
sta SET_VAR_OFFSET,y

cmp INC_NEXT_UNIT_AFTER
bcs @timeHasPassed
lda #$0
rts

@timeHasPassed:
lda #$0
sta SET_VAR_OFFSET,y
lda #$1
rts

b6TimerTick:
clc
lda b6Hund
adc #TIMER_TICK
sta b6Hund

cmp #60
bcs @clearHundreds
rts

@clearHundreds:
stz b6Hund

@seconds:
lda #60
sta INC_NEXT_UNIT_AFTER

lda #SECONDS_VAR
sta TIMER_VAR
jsr b6HandleTimeComponent
beq @end

@minutes:
lda #MINUTES_VAR
sta TIMER_VAR
jsr b6HandleTimeComponent
beq @end

@hours:
lda #HOURS_VAR
sta TIMER_VAR
lda #24
sta INC_NEXT_UNIT_AFTER
jsr b6HandleTimeComponent
beq @end

@days:
lda #DAYS_VAR
sta TIMER_VAR
lda #255 ;There is no unit higher than days, so just set to a very high amount
sta INC_NEXT_UNIT_AFTER
jsr b6HandleTimeComponent
beq @end

@end:
rts
b6Hund: .byte $0
.endif