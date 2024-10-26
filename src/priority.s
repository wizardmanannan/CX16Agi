.segment "BANKRAM08"

.ifndef  PRIORITY_INC

PRIORITY_INC = 1

NOT_AN_OBSTACLE = 4
WATER = 3
CONTROL_LINES = 2
HIGHEST_BOUNDARY = 1
LOWEST_BOUNDARY = 0
;byte b8GetPriority(byte X, byte Y)
;Note that all AGI implementations draw the control lines over the top of the priority screen
;Original MEKA splits the priority screen into priority and control screens, but in a limited memory system this is wasteful.
;Therefore we calculate the control value at the very time it is needed.
;Here is the algorithm, regarding the pixel at priority screen x, y:
;If the pixel is >= 4 (priority not control) => Return NOT_AN_OBSTACLE
;If the pixel is < 4 (control) => pixel
_b8GetControl:
.scope
X_VAL = ZP_TMP_2
Y_VAL = ZP_TMP_2 + 1

sta Y_VAL
jsr popa
sta X_VAL

sei
ldy Y_VAL
CALC_VRAM_ADDR_PRIORITY X_VAL, #$0
ldx VERA_data0
REENABLE_INTERRUPTS

lda X_VAL
lsr
bcc @getEvenValue

@getOddValue:
txa 
and #$0F
bra @checkValue

@getEvenValue:
txa
lsr
lsr
lsr
lsr

@checkValue:
cmp #NOT_AN_OBSTACLE ;Any thing greater than or equal to this value is a priority value and not a control value and therefore not an obstacle 

bcc @return
lda #4

@return:
rts
.endscope


.endif