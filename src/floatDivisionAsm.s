.segment "CODE"
_floatDivision:
bra @start
@numerator: .word $0 ; Even though numerator is only one byte we double it for address looked up
@denominator: .word $0 ; Even though denominator is only one byte we double it for address looked up
@originalZPCh: .word $0 ;For Division Bank Table
@originalZPDisp: .word $0 ;For Division Address Table
@previousRamBank: .byte $0
@start:
dec
dec
sta @denominator

jsr popa
dec
sta @numerator

lda RAM_BANK
sta @previousRamBank

lda ZP_DIV_BANK
sta @originalZPCh
lda ZP_DIV_BANK+1
sta @originalZPCh+1

lda ZP_DIV_ADDR
sta @originalZPDisp
lda ZP_DIV_ADDR+1
sta @originalZPDisp+1

lda @numerator
clc
adc ZP_DIV_BANK
sta ZP_DIV_BANK
lda #$0
adc ZP_DIV_BANK + 1
sta ZP_DIV_BANK + 1

lda #DIVISION_METADATA_BANK
sta RAM_BANK

lda (ZP_DIV_BANK)
tax

lda @originalZPCh
sta ZP_DIV_BANK

lda @originalZPCh + 1
sta ZP_DIV_BANK + 1


lda @numerator
clc
asl 
sta @numerator
lda #$0 ; always zero
rol
sta @numerator+1

lda @numerator
clc
adc ZP_DIV_ADDR
sta ZP_DIV_ADDR
lda @numerator+1
adc ZP_DIV_ADDR + 1
sta ZP_DIV_ADDR + 1

lda (ZP_DIV_ADDR)
sta ZP_DIV_AREA
ldy #$1
lda (ZP_DIV_ADDR),y
sta ZP_DIV_AREA+1

lda @originalZPDisp
sta ZP_DIV_ADDR
lda @originalZPDisp + 1
sta ZP_DIV_ADDR + 1

lda @denominator
pha
clc
asl 
sta @denominator
lda #$0 ; always zero
rol
sta @denominator+1

pla
clc
adc @denominator
sta @denominator
lda #$0
adc @denominator+1
sta @denominator+1

lda @denominator
clc
adc ZP_DIV_AREA
sta ZP_DIV_AREA
lda @denominator+1
adc ZP_DIV_AREA+1
sta ZP_DIV_AREA+1

stx RAM_BANK
stz sreg + 1
ldy #$1
lda (ZP_DIV_AREA),y
tax
ldy #$2
lda (ZP_DIV_AREA),y
sta sreg
lda (ZP_DIV_AREA)

ldy @previousRamBank
sty RAM_BANK

rts