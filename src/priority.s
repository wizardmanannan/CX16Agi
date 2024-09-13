.segment "BANKRAM08"

.ifndef  PRIORITY_INC

PRIORITY_INC = 1

;byte b8GetPriority(byte X, byte Y)
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
cmp #$4

bcc @return
lda #4

@return:
rts
.endscope


.endif