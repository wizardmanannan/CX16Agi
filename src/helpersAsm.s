.ifndef  HELPERS_INC

HELPERS_INC = 1

.segment "CODE"

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