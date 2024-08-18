.ifndef  FILL_STACK_INC

.segment "BANKRAM08"

FILL_STACK_INC = 1
.include "global.s"
FILL_STACK_POINTER = ZP_TMP_4

FILL_STACK_TMP = ZP_TMP_4 + 1
STACK_SIZE = 256

b8FillStackLx: .res STACK_SIZE
b8FillStackRx: .res STACK_SIZE
b8FillStackY: .res STACK_SIZE

;VERBOSE_STACK = 1

.ifdef VERBOSE_STACK
calledPush: .asciiz "called push %d %d y%d"
calledPop: .asciiz "called pop %d %d %d"
.endif

.macro FILL_STACK_PUSH
.ifdef VERBOSE_STACK
phy
phx
pha

lda #<calledPush
ldx #>calledPush
jsr pushax

tsx
inx
lda STACK_POINTER,x
ldx #$0
jsr pushax

tsx
inx
inx
lda STACK_POINTER,x
ldx #$0
jsr pushax

tsx
inx
inx
inx
lda STACK_POINTER,x
ldx #$0
jsr pushax

ldy #8

jsr _printfSafe
PRINT_NEW_LINE

pla
plx
ply
.endif
stx TMP

ldx FILL_STACK_POINTER

sta b8FillStackLx,x

lda TMP
sta b8FillStackRx,x

tya
sta b8FillStackY,x

inc FILL_STACK_POINTER
.endmacro


.macro FILL_STACK_POP LX, RX, Y_VAL
.local @end
lda #$0
ldx FILL_STACK_POINTER
beq @end

dex
stx FILL_STACK_POINTER

lda b8FillStackLx,x
sta LX

lda b8FillStackRx,x
sta RX

lda b8FillStackY,x
sta Y_VAL


.ifdef VERBOSE_STACK
lda #< calledPop
ldx #> calledPop
jsr pushax

lda LX
ldx #$0
jsr pushax

lda RX
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

ldy #8

jsr _printfSafe
PRINT_NEW_LINE
.endif

lda #$1
@end:
.endmacro

.endif
