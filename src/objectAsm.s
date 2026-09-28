.include "x16.inc"

.ifndef OBJECT_INC
OBJECT_INC = 1

.import _b5FlushBuffer
.import _bDObjectNameLengths

.import _bDObjectRowPaletteAddresses
.import _bDSelectedObject
.import _bDUnselectedObject
.import _offsetOfLengthOffset
.import _offsetOfObjectNum
.import _bDSelectedScreenObjNum
.import _bDUnselectedScreenObjNum
.segment "BANKRAM0D"

; WRITE_NEXT
; Writes one byte from A to the current output pointer (WRITE_ZP).
; If the pointer has reached the end of the golden-RAM work area,
; flushes the buffer via banked helper _b5FlushBuffer and resets
; WRITE_ZP to the start of GOLDEN_RAM_WORK_AREA, then writes.
.macro WRITE_NEXT
ldx WRITE_ZP + 1
cpx #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @highByteCheck

@write:
sta (WRITE_ZP)          ; store the byte at the current write pointer
inc WRITE_ZP
beq @incHigh
rts
@incHigh:
inc WRITE_ZP + 1        ; crossed a page; bump high byte of pointer
@return:
rts

@highByteCheck:
ldx WRITE_ZP
cpx #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write

@refreshBuffer:
pha                     ; preserve the byte we still need to write
lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1
TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer   ; flush full work-area buffer

lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1        ; reset write pointer to start of work area
pla
jmp @write
.endmacro

; #define WRITE_NEXT(toWrite)  \
;     do {                              \
;        if(data >= GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE) \
; 		{ \
; 			b5FlushBuffer(&bufferStatus); \
;             data = GOLDEN_RAM_WORK_AREA; \
; 		} \
; 		 *data++ = toWrite; \
;         \
;     } while(0);

WRITE_ZP = ZP_TMP_10                ; current write pointer into work area
BUFFER_STATUS_ADDRESS = ZP_TMP_12   ; pointer to buffer-status struct for flush
OBJECT_NAME = ZP_TMP_13             ; pointer to current object-name string
CH = ZP_TMP_14                      ; current character being copied
OBJ_NAME_OFFSET = ZP_TMP_14 + 1     ; saved Y while using Y as dest offset
I_COUNTER = ZP_TMP_16               ; object-index / outer counter
J_COUNTER = ZP_TMP_16 + 1           ; column / pad-length counter
LASTLENGTH = ZP_TMP_17              ; running length of the current inventory line

; _bDWriteNext
; C-callable wrapper: writes A using the WRITE_NEXT macro and returns.
_bDWriteNext:
WRITE_NEXT

; bDDisplayInventoryInnerWithCompare
; Copies the NUL-terminated object name at (OBJECT_NAME) into the
; work-area buffer, checking after each store whether the write
; pointer has hit the end of GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE.
; Flushes and resets the pointer if it has. Used when the remaining
; room in the buffer is not known to be large enough for the name.
bDDisplayInventoryInnerWithCompare:
lda (OBJECT_NAME)
sta CH
beq @return             ; empty name — nothing to write

ldx #$0                 ; X = bytes written this call
ldy #$0                 ; Y = index into object-name string
@loop:
lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @highByteCheck      ; high byte already at/past limit — check low byte

@write:
sty OBJ_NAME_OFFSET     ; save string index
txa
tay                     ; Y = dest offset from current WRITE_ZP
lda CH
sta (WRITE_ZP),y        ; write character
ldy OBJ_NAME_OFFSET     ; restore string index

iny
inx

@getObjectName:
lda (OBJECT_NAME),y
sta CH
bne @loop               ; more characters in the name

clc
txa
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1        ; advance WRITE_ZP by number of bytes written
bra @return

@highByteCheck:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @write              ; still room in this page

@refreshBuffer:
lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1

TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer

lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1        ; reset pointer after flush

ldx #$0                 ; dest offset restarts at 0 in the fresh buffer
jmp @write



@return:
rts

; _bDDisplayInventoryInner
; Fast path for copying an object name when the remaining buffer space
; is known (or assumed) to be enough. Self-modifies the store address
; from WRITE_ZP, copies until NUL, then advances WRITE_ZP.
; If the high (and then low) byte of WRITE_ZP shows the buffer may
; overflow, jumps to bDDisplayInventoryInnerWithCompare instead.
;
; On entry: A = additional length already accounted for on this line
;           (added to WRITE_ZP and compared against the work-area end).
_bDDisplayInventoryInner:

clc
adc WRITE_ZP
sta sreg
lda #$0
adc WRITE_ZP + 1
sta sreg + 1            ; sreg = WRITE_ZP + incoming length (end probe)

cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE) 
bcc @checkLowByte

@noCompareNeeded:
lda (OBJECT_NAME)
beq @return             ; empty name

ldx WRITE_ZP
stx @inventoryInnerWrite + 1
ldx WRITE_ZP + 1
stx @inventoryInnerWrite + 2   ; patch store address to current WRITE_ZP

ldx #$0                 ; dest offset
ldy #$0                 ; source index
@loop:
@inventoryInnerWrite:
sta GOLDEN_RAM_WORK_AREA,x     ; self-modified: writes to WRITE_ZP,X
iny
inx
@getObjectName:
lda (OBJECT_NAME),y
bne @loop               ; continue until NUL

clc
txa
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1        ; advance write pointer by bytes copied

@return:
rts
@checkLowByte:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE) 
bcc @noCompareNeeded
jmp bDDisplayInventoryInnerWithCompare   ; not enough guaranteed room


; bDPadWordsWithSpacesWithCompare
; Pads the current inventory line with spaces until J_COUNTER reaches
; TILES_ACROSS, checking the write pointer against the work-area limit
; on every store and flushing/resetting when full.
bDPadWordsWithSpacesWithCompare:
; cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
; beq @checkLow
; bcs @checkLoopCondition ;Don't think we need this because i don't think j can be that big
; jmp bDPadWordsWithSpacesNoCompare
; @checkLow:
; lda sreg
; cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
; bcs @checkLoopCondition
; jmp bDPadWordsWithSpacesNoCompare

; bra @checkLoopCondition
@loop:
inc J_COUNTER
@checkLoopCondition:
lda J_COUNTER
cmp #TILES_ACROSS
bcs @endLoop            ; line is already a full row of tiles

lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcs @lowByteCheckSpace

@writeSpace:
 lda #SPACE
 sta (WRITE_ZP)
 inc WRITE_ZP
 bne @loop

@incHighSpace:
inc WRITE_ZP + 1        ; page crossed while writing a space
bra @loop

@lowByteCheckSpace:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE)
bcc @writeSpace

lda BUFFER_STATUS_ADDRESS
ldx BUFFER_STATUS_ADDRESS + 1
TRAMPOLINE #HELPERS_BANK, _b5FlushBuffer
;                 *WRITE_ZP_PTR = GOLDEN_RAM_WORK_AREA;
lda #<GOLDEN_RAM_WORK_AREA
sta WRITE_ZP
lda #>GOLDEN_RAM_WORK_AREA
sta WRITE_ZP + 1        ; reset after flush
jmp @writeSpace;
@endLoop:
rts

; _bDPadWordsWithSpaces
; Pads the current inventory line with spaces so the next object name
; starts on a TILES_ACROSS column boundary.
;
; On entry (cc65 calling convention):
;   A  = lastLength  (bytes already used on this line)
;   stacked byte = i  (object index; used to look up name length)
;
; Computes J_COUNTER = lastLength + objectNameLengths[i], then writes
; SPACE until J_COUNTER == TILES_ACROSS. Fast path when there is
; clearly enough room in the work area; otherwise jumps to the
; compare-and-flush variant.
_bDPadWordsWithSpaces:
sta LASTLENGTH
jsr popa 
sta I_COUNTER           ; I_COUNTER = object index

@loopInit:
tay
lda _bDObjectNameLengths,y
clc
adc LASTLENGTH
sta J_COUNTER           ; column we would occupy after this name

clc
lda #<GOLDEN_RAM_WORK_AREA
adc J_COUNTER
sta sreg
lda #>GOLDEN_RAM_WORK_AREA
adc #$0                 ; probe address (high byte left in A)

lda WRITE_ZP + 1
cmp #>(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE - TILES_ACROSS)
bcs @checkLowByte       ; may not have a full row of room left

@noCompareNeeded:
ldy #$0                 ; dest offset from WRITE_ZP
ldx J_COUNTER           ; current column

@checkLoopCondition:
cpx #TILES_ACROSS
bcs @endLoop

@writeSpace:
lda #SPACE
sta (WRITE_ZP),y
iny
inx
bra @checkLoopCondition

@endLoop:

clc
tya
adc WRITE_ZP
sta WRITE_ZP
lda #$0
adc WRITE_ZP + 1
sta WRITE_ZP + 1        ; advance WRITE_ZP by number of spaces written

tya
rts

@checkLowByte:
lda WRITE_ZP
cmp #<(GOLDEN_RAM_WORK_AREA + LOCAL_WORK_AREA_SIZE - TILES_ACROSS)
bcc @noCompareNeeded
jmp bDPadWordsWithSpacesWithCompare

OBJECT_TO_SET_PALETTE = IRQ_TMP_1
OBJECT_VERA_ADDRESS = IRQ_TMP_2
OBJECT_VERA_ADDRESS_HIGH = IRQ_TMP_3 ;Only one high byte
OBJECT_NAME_LENGTH = IRQ_TMP_3 + 1
PALETTE_TO_SET = IRQ_TMP_4
SCREEN_OBJ_NUM = IRQ_TMP_4 + 1
bDSetInventoryRowPalette:
GET_STRUCT_8_STORED_OFFSET _offsetOfLengthOffset, OBJECT_TO_SET_PALETTE
asl
sta OBJECT_VERA_ADDRESS

; GET_STRUCT_8_STORED_OFFSET _offsetOfRow, OBJECT_TO_SET_PALETTE
; stp
; asl
lda SCREEN_OBJ_NUM
lsr ;Why are we halfing and then doubling? This is sensible, halfing gets up the row via integer maths for example half of 3 is 1. Then you need to double for the 16 bit address for row 1 which is at position 2.
asl

tax 
lda _bDObjectRowPaletteAddresses,x
tay
lda _bDObjectRowPaletteAddresses + 1,x 
tax

clc
tya
adc OBJECT_VERA_ADDRESS
sta OBJECT_VERA_ADDRESS
txa
adc #$0
sta OBJECT_VERA_ADDRESS + 1

stz OBJECT_VERA_ADDRESS_HIGH
stz OBJECT_VERA_ADDRESS_HIGH + 1

SET_VERA_ADDRESS OBJECT_VERA_ADDRESS, #$2, OBJECT_VERA_ADDRESS_HIGH, #$0

GET_STRUCT_8_STORED_OFFSET _offsetOfObjectNum,OBJECT_TO_SET_PALETTE
tax
lda _bDObjectNameLengths,x

tax
lda PALETTE_TO_SET
@setPaletteLoop:
sta VERA_data0
dex
bne @setPaletteLoop
@endLoop:


rts

OBJECT_SELECTED_PALETTE = $20
OBJECT_UNSELECTED_PALETTE = $10

NOTHING_TO_SELECT = $FF

bDHighlightInventoryRow:

lda #OBJECT_SELECTED_PALETTE
sta PALETTE_TO_SET
lda _bDSelectedObject
sta OBJECT_TO_SET_PALETTE
lda _bDSelectedObject + 1
sta OBJECT_TO_SET_PALETTE + 1
lda _bDSelectedScreenObjNum
sta SCREEN_OBJ_NUM
jsr bDSetInventoryRowPalette

lda _bDUnselectedScreenObjNum
cmp #NOTHING_TO_SELECT
beq @return

lda #OBJECT_UNSELECTED_PALETTE
sta PALETTE_TO_SET
lda _bDUnselectedObject
sta OBJECT_TO_SET_PALETTE
lda _bDUnselectedObject + 1
sta OBJECT_TO_SET_PALETTE + 1
lda _bDUnselectedScreenObjNum
sta SCREEN_OBJ_NUM
jsr bDSetInventoryRowPalette

@return:
rts



.endif