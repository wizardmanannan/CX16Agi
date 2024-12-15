.ifndef  HELPERS_INC
.importzp sreg

HELPERS_INC = 1
.segment "BANKRAM05"

multBy0: .byte $00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00,$00
multBy1: .byte $00,$01,$02,$03,$04,$05,$06,$07,$08,$09,$0a,$0b,$0c,$0d,$0e,$0f
multBy2: .byte $00,$02,$04,$06,$08,$0a,$0c,$0e,$10,$12,$14,$16,$18,$1a,$1c,$1e
multBy3: .byte $00,$03,$06,$09,$0c,$0f,$12,$15,$18,$1b,$1e,$21,$24,$27,$2a,$2d
multBy4: .byte $00,$04,$08,$0c,$10,$14,$18,$1c,$20,$24,$28,$2c,$30,$34,$38,$3c
multBy5: .byte $00,$05,$0a,$0f,$14,$19,$1e,$23,$28,$2d,$32,$37,$3c,$41,$46,$4b
multBy6: .byte $00,$06,$0c,$12,$18,$1e,$24,$2a,$30,$36,$3c,$42,$48,$4e,$54,$5a
multBy7: .byte $00,$07,$0e,$15,$1c,$23,$2a,$31,$38,$3f,$46,$4d,$54,$5b,$62,$69
multBy8: .byte $00,$08,$10,$18,$20,$28,$30,$38,$40,$48,$50,$58,$60,$68,$70,$78
multBy9: .byte $00,$09,$12,$1b,$24,$2d,$36,$3f,$48,$51,$5a,$63,$6c,$75,$7e,$87
multBy10: .byte $00,$0a,$14,$1e,$28,$32,$3c,$46,$50,$5a,$64,$6e,$78,$82,$8c,$96
multBy11: .byte $00,$0b,$16,$21,$2c,$37,$42,$4d,$58,$63,$6e,$79,$84,$8f,$9a,$a5
multBy12: .byte $00,$0c,$18,$24,$30,$3c,$48,$54,$60,$6c,$78,$84,$90,$9c,$a8,$b4
multBy13: .byte $00,$0d,$1a,$27,$34,$41,$4e,$5b,$68,$75,$82,$8f,$9c,$a9,$b6,$c3
multBy14: .byte $00,$0e,$1c,$2a,$38,$46,$54,$62,$70,$7e,$8c,$9a,$a8,$b6,$c4,$d2
multBy15: .byte $00,$0f,$1e,$2d,$3c,$4b,$5a,$69,$78,$87,$96,$a5,$b4,$c3,$d2,$e1 

multLookUp: 
.addr multBy0
.addr multBy1
.addr multBy2
.addr multBy3
.addr multBy4
.addr multBy5
.addr multBy6 
.addr multBy7 
.addr multBy8 
.addr multBy9 
.addr multBy10 
.addr multBy11 
.addr multBy12 
.addr multBy13 
.addr multBy14 
.addr multBy15

.segment "CODE"

;Multiplies two products from 0 to 15
;Input a/y, output a
mul8x8to8:

ldx RAM_BANK
phx

ldx #HELPERS_BANK
stx RAM_BANK

asl
tax
lda multLookUp,x
sta sreg
lda multLookUp + 1,x
sta sreg + 1

lda (sreg),y

plx
stx RAM_BANK
rts

.export _trampoline
.import popa
.importzp tmp4
.import callptr4

.macro REFRESH_BUFFER_ABS BUFFER_POINTER, BUFFER_STATUS, ABS_BUFFER_STATUS
txa
pha ;Preserve x and y
tya
pha

lda BUFFER_STATUS
ldx BUFFER_STATUS + 1

TRAMPOLINE #HELPERS_BANK, _b5RefreshBuffer
lda #< GOLDEN_RAM_WORK_AREA
sta BUFFER_POINTER
lda #> GOLDEN_RAM_WORK_AREA
sta BUFFER_POINTER + 1

pla
tay ;Restore x and y
pla
tax

.endmacro

.macro GET_NEXT_ABS BUFFER_POINTER, BUFFER_STATUS, ABS_BUFFER_STATUS
.local @getNext
.local @return
lda BUFFER_POINTER
cmp #<(GOLDEN_RAM_WORK_AREA_END + 1)
bne @getNext
lda BUFFER_POINTER + 1
cmp #>(GOLDEN_RAM_WORK_AREA_END + 1)
bne @getNext

REFRESH_BUFFER_ABS BUFFER_POINTER, BUFFER_STATUS

@getNext:
lda (BUFFER_POINTER)
inc BUFFER_POINTER
bne @return
inc BUFFER_POINTER + 1

@return:
.endmacro
.macro REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS
txa
pha ;Preserve x and y
tya
pha

lda #< BUFFER_STATUS
ldx #> BUFFER_STATUS
TRAMPOLINE #HELPERS_BANK, _b5RefreshBuffer
lda #< GOLDEN_RAM_WORK_AREA
sta BUFFER_POINTER
lda #> GOLDEN_RAM_WORK_AREA
sta BUFFER_POINTER + 1

pla
tay ;Restore x and y
pla
tax

.endmacro

.macro GET_NEXT BUFFER_POINTER, BUFFER_STATUS
.local @getNext
.local @return
lda BUFFER_POINTER
cmp #<(GOLDEN_RAM_WORK_AREA_END + 1)
bne @getNext
lda BUFFER_POINTER + 1
cmp #>(GOLDEN_RAM_WORK_AREA_END + 1)
bne @getNext

REFRESH_BUFFER BUFFER_POINTER, BUFFER_STATUS

@getNext:
lda (BUFFER_POINTER)
inc BUFFER_POINTER
bne @return
inc BUFFER_POINTER + 1

@return:
.endmacro
;Assumes args are in a/x and C stack
.macro TRAMPOLINE BANK, JMPTO

ldy BANK
sty tmp4

ldy #< JMPTO
sty ptr4
ldy #> JMPTO
sty ptr4 + 1

jsr _trampoline
.endmacro

.import _debugBank
;void trampolineDebug(void* callPtr)
;Used to allow C code to trampoline to the debug bank. Make sure debugging is enabled when calling otherwise you will jump into space. It is done deliberately so as to not waste low RAM
_trampolineDebug:
.ifdef DEBUG
sta ptr4
stx ptr4 + 1

lda _debugBank
sta tmp4
jsr _trampoline
rts
.endif

;Assumes args are in a/x and C stack
_trampoline:
sta @aVal ;Preserve a argument

lda RAM_BANK
pha

lda tmp4
sta RAM_BANK

lda @aVal
jsr callptr4
sta @aVal ;Perserve return a

pla
sta RAM_BANK

lda @aVal
rts
@aVal: .byte $0


.endif