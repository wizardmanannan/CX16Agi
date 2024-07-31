.segment "BANKRAM08"

.ifndef  FILL_INC

FILL_INC = 1

.include "lineDrawing.s"
.include "fillStack.s"

color_table:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

mask_table:
    .byte %11111111 ; 0 $00
    .byte %11111100 ; 1 $03
    .byte %11110000 ; 2 $0F
    .byte %11000000 ; 3 $3F
    .byte %11111111; 0 $00


.macro pop_c_stack addr
    ldy #$00
    lda (C_STACK_ADDR),y  ; Get the parameter from the C-Stack
    sta addr
    ; move the C-Stack pointer up by 1
    inc C_STACK_ADDR
    bne *+4  ; If no overflow, skip the next instruction
    inc C_STACK_ADDR + 1
.endmacro

.import _picColour
.import _priDrawEnabled
.macro can_fill x_val, y_val
.scope
    ; registers X and Y contain pixel coordinates
    ; returns 0 in A register if the pixel cannot be filled (early exit)
    ; returns 1 in A register if the pixel can be filled
    VIS_PIXEL = ZP_TMP_16
    PRI_PIXEL = ZP_TMP_16 + 1
    TMP = ZP_TMP_17

    lda x_val 
    ldy y_val
    ; get the vis pixel at the current x and y
    CALC_VRAM_ADDR_LINE_DRAW_160 x_val, y_val

    lda VERA_data0
    and #$0F ; mask out the top 4 bits
    sta VIS_PIXEL

    ; get the pri pixel at the current x and y
    ; Add 0x8000 to the VERA::ADDR
    lda VERA_addr_high
    clc
    adc #$80
    sta VERA_addr_high
    lda VERA_addr_bank
    adc #$00
    sta VERA_addr_bank
    lda VERA_data0
    and #$0F ; mask out the top 4 bits
    sta PRI_PIXEL

    ; is the current vis colour 15 (white)?
    ; if (vis_colour == 15) return 0;
    lda _picColour
    cmp #15
    beq @cannot_fill

    ; is priority disabled and the current vis pixel not white?
    ; if (!pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    bne @pri_enabled_check ; if priority is enabled, skip this check
    lda VIS_PIXEL
    cmp #15
    bne @cannot_fill

@pri_enabled_check:
    ; is priority enabled and vis disabled and the current pri pixel not red?
    ; if (pri_enabled && !vis_enabled && (asm_get_pri_pixel(x, y) != 4)) return 0;
    lda _picDrawEnabled
    bne @vis_enabled_check
    lda PRI_PIXEL
    cmp #4
    bne @cannot_fill

@vis_enabled_check:
    ; is priority enabled and the current vis pixel not white?
    ; if (pri_enabled && (asm_get_vis_pixel(x, y) != 15)) return 0;
    lda _priDrawEnabled
    beq @can_fill
    lda VIS_PIXEL
    cmp #15
    bne @cannot_fill

@can_fill:
    lda #1 ; return 1 (pixel can be filled)
    ldx #0 ; clear X register
    bra @end_macro

@cannot_fill:
    lda #0 ; return 0 (pixel cannot be filled)

@end_macro: 

.endscope
.endmacro

;void b8ScanAndFill(uint8_t x, uint8_t y)
;{
;    static uint8_t lx, rx;
;
;    ;printfSafe("in. trying to fill %d, %d\n", x,y);
;
;    ; Inline can_fill logic at the start to avoid unnecessary function calls
;    if (b8AsmCanFill(x, y) == false) {
;#ifdef VERBOSE_FILL
;        printfSafe("blocked on %d %d\n", x, y);
;#endif ; VERBOSE_FILL
;        return;
;    }
;
;#ifdef VERBOSE_FILL
;    printfSafe("can fill true %d %d\n", x, y);
;#endif
;
;    lx = x;
;    rx = x;
;
;    ;printfSafe("at 1\n");
;
;    ; Inline can_fill logic for left expansion
;    while (lx != 0) {
;        if (b8AsmCanFill(lx - 1, y) == false) {
;            break;
;        }
;        --lx;
;    }
;
;    ;printfSafe("at 2\n");
;
;    ; Inline can_fill logic for right expansion
;    while (rx != 159) {
;        if (b8AsmCanFill(rx + 1, y) == false) {
;            
;#ifdef VERBOSE_FILL
;            printfSafe("stopping at %d\n", rx);
;#endif
;            break;
;        }
;        ++rx;
;        ;printfSafe("l2 rx %d\n", rx);
;    }
;
;    ;printfSafe("at 3. x0 %d x1 %d y %d color %d\n", lx, rx + 1, y, picColour);
;
;    ; pset_hline(lx, rx, y);
;#ifdef VERBOSE_FILL
;        printfSafe("%d drawing a line %p, %p to %p, %p\n",drawCounter++, lx, y, rx, y);
;#endif
;        
;        if (drawCounter == 84)
;        {
;            enableStop = TRUE;
;        }
;        
;        if (picDrawEnabled)
;        {
;            b8AsmPlotVisHLineFast(lx, rx, y, picColour);
;        }
;        enableStop = FALSE;
;
;    ;printfSafe("at 4\n");
;
;   /* if (priDrawEnabled)
;        asm_plot_pri_hline_fast((lx << 1), (rx << 1) + 2, y + STARTING_BYTE, priColour);*/
;
;    ; if (y != 167) {
;    ;     push(lx, rx, y + 1, 1); ; push below
;    ; }
;    ; if (y != 0) {
;    ;     push(lx, rx, y - 1, -1); ; push above
;    ; }
;
;        if (y < PICTURE_HEIGHT - 1)
;        {
;            b8Push(lx, rx, y + 1); ; push below
;        }
;
;        if (y > 0)
;        {
;            b8Push(lx, rx, y - 1); ; push above
;        }
;}

;VERBOSE_FILL = 1
.import _printfSafe
.ifdef VERBOSE_FILL
tryingToFill: .asciiz "in. trying to fill %d, %d"
blockedOn: .asciiz "blocked on %d %d"
canFillTrue: .asciiz "can fill true %d %d"
drawLine: .asciiz "%d drawing a line %p, %p to %p, %p"

drawCounter: .word $0
.endif

.macro b8ScanAndFill
.scope
X_VAL = ZP_TMP_10
Y_VAL = ZP_TMP_10 + 1
LX = ZP_TMP_12
RX = ZP_TMP_12 + 1
SCAN_FILL_TMP = ZP_TMP_13

sty Y_VAL
stx X_VAL

.ifdef VERBOSE_FILL
lda #<tryingToFill
ldx #>tryingToFill
jsr pushax

lda X_VAL
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

ldy #6

jsr _printfSafe
PRINT_NEW_LINE
.endif

can_fill X_VAL, Y_VAL
cmp #$0
bne @expansion
jmp cannot_fill

@expansion:
lda X_VAL
sta LX
sta RX

;while (lx != 0)
bne leftExpansionLoop
jmp endLeftExpansionLoop

leftExpansionLoop:

; if (b8AsmCanFill(lx - 1, y) == false break;

lda LX
dec
sta SCAN_FILL_TMP
can_fill SCAN_FILL_TMP, Y_VAL
cmp #$0
beq endLeftExpansionLoop

;--lx
lda LX
dec
sta LX

beq endLeftExpansionLoop
jmp leftExpansionLoop
endLeftExpansionLoop:

.ifdef VERBOSE_FILL
lda #< canFillTrue
ldx #> canFillTrue
jsr pushax

lda X_VAL
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

ldy #6

jsr _printfSafe
PRINT_NEW_LINE
.endif

;while (rx != 159) {
lda RX
rightExpansionLoopCheck:
cmp #PICTURE_WIDTH - 1
bne rightExpansionLoop
jmp endRightExpansionLoop
rightExpansionLoop:
;if (b8AsmCanFill(rx + 1, y) == false) break
lda RX
inc
sta SCAN_FILL_TMP
nop
can_fill SCAN_FILL_TMP,Y_VAL
cmp #$0
beq endRightExpansionLoop
inc RX ;rx++

lda RX
cmp #PICTURE_WIDTH - 1 ;A duplicate of the while loop condition which saves a jump
beq endRightExpansionLoop

jmp rightExpansionLoop
endRightExpansionLoop:

.ifdef VERBOSE_FILL
lda #< drawLine
ldx #> drawLine
jsr pushax

lda drawCounter
ldx drawCounter + 1
jsr pushax

lda LX
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

lda RX
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

ldy #12

jsr _printfSafe
PRINT_NEW_LINE

inc drawCounter
bne @afterDrawCounterInc
@drawCounterHighByte:
inc drawCounter + 1

@afterDrawCounterInc:
.endif
;if (picDrawEnabled)
;        {
;            b8AsmPlotVisHLineFast(lx, rx, y, picColour);
;        }

lda _picDrawEnabled
beq @pushBelow
ldx LX
lda RX
jsr pushax
lda Y_VAL
jsr pusha 
lda _picColour

jsr _b8AsmPlotVisHLineFast

;JSRFAR _b5WaitOnKey, 5

@pushBelow:
;        if (y < PICTURE_HEIGHT - 1)
;        {
;            b8Push(lx, rx, y + 1); ; push below
;        }
;
ldy Y_VAL
cpy #PICTURE_HEIGHT - 1
bcs @pushAbove
lda LX
ldx RX
iny
FILL_STACK_PUSH
@pushAbove:
;        if (y > 0)
;        {
;            b8Push(lx, rx, y - 1); ; push above
;        }
ldy Y_VAL
beq @return
lda LX
ldx RX
dey
FILL_STACK_PUSH

@return:
.import _b5WaitOnKey
jmp end
cannot_fill:

.ifdef VERBOSE_FILL
lda #< blockedOn
ldx #> blockedOn
jsr pushax

lda X_VAL
ldx #$0
jsr pushax

lda Y_VAL
ldx #$0
jsr pushax

ldy #6

jsr _printfSafe
PRINT_NEW_LINE
.endif
end:
.endscope
.endmacro

temp            = ZP_TMP_2
color           = ZP_TMP_2 + 1
start_amount    = ZP_TMP_3
start_mask      = ZP_TMP_5
end_mask        = ZP_TMP_5 + 1
length_low      = ZP_TMP_6
Y0              = ZP_TMP_7
X1_LOW          = ZP_TMP_7 + 1
X0_LOW          = ZP_TMP_8 + 1
; asm_plot_vis_hline(unsigned short x0, unsigned short x1, unsigned char y, unsigned char color);
; plots 2 pixels at a time for 160x200 mode
.macro b8AsmPlotVisHLine
    
    ; Line is a short line
    lda #%00000100  ; DCSEL = Mode 2 
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing

    lda X0_LOW
    lda X1_LOW
    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_LOW, Y0


    lda #$10    ; Enable auto-increment
    sta VERA_addr_bank

    ; Calculate the line length and the loop count / 2      
    lda X1_LOW
    sec
    sbc X0_LOW
    tax
    ; lda X1_HIGH
    ; sbc X0_HIGH
    ; lsr             ; shift right but throw away result
    ; txa
    ; ror             ; rotate into low byte
    ; tax

    ldy color
    lda color_table, y
    
    @loop:
        ; Plotting action 
        sta VERA_data0
        dex
        bne @loop

.endmacro ; _plot_vis_hline

shortLine:
b8AsmPlotVisHLine
rts ; Return from subroutine
b8AsmPlotVisHLineJump:
jmp shortLine

.proc _b8AsmPlotVisHLineFast
    TMP = ZP_TMP_9 + 1

    ; color is in A register
    sta color

    pop_c_stack Y0
    pop_c_stack X1_LOW
    pop_c_stack X0_LOW
    
    ; Calculate the line length and the loop count
    ; Ensure X1 >= X0 
    inc X1_LOW
    lda X1_LOW
    sec
    sbc X0_LOW
    sta length_low 
    lda length_low
    cmp #$10
    bcc b8AsmPlotVisHLineJump
long_line:
    ; Change DCSEL to mode 6 for cache write operations
    lda #%00001100
    sta VERA_ctrl

    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_LOW, Y0
    
    ldx color
    lda color_table, x
    tay
    sta $9f29
    sta $9f2A
    sta $9f2B
    sta $9f2C

    stz VERA_ctrl
    lda #%10000
    sta VERA_addr_bank

    ldx VERA_addr_low
    @nonDivideByFourLoopCheck: 
    txa
    and #3
    beq @endLoop
    sty VERA_data0
    inx
    dec length_low
    bra @nonDivideByFourLoopCheck
    @endLoop:

    ; Set up VERA for cache operations
    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    lda #%01000000  ; Enable cache writing
    sta VERA_dc_video

    lda length_low
    and #3
    tax 
    lda mask_table,x
    sta end_mask

    ; Calculate the number of full 8-pixel (32-bit) chunks by dividing by 8 (shift right 3 times)
    lsr length_low    ; Rotate right the low byte through carry
    lsr length_low    ; Rotate right again
  

    ; Set address auto-increment to 4 bytes
    lda #%00110000
    sta VERA_addr_bank

      ; Loop counter
    lda #%0 ; clear the mask
    ldx length_low

@loop:
    ; Plotting action
    sta VERA_data0
    dex
    bne @loop 

done_plotting:
    ; Handle the last partial chunk 

    lda end_mask
    sta VERA_data0

    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing
    
    rts                     ; Return from subroutine
.endproc ; _plot_vis_hline_fast

.proc _b8AsmCanFill
    X_VAL = ZP_TMP_14
    Y_VAL = ZP_TMP_14 + 1

    sta Y_VAL
    jsr popa
    sta X_VAL

    can_fill X_VAL, Y_VAL

    rts ; return
.endproc

.proc _b8AsmFloodFill
    LX      = ZP_TMP_17 + 1
    RX      = ZP_TMP_18
    Y1      = ZP_TMP_18 + 1
    NX      = ZP_TMP_19 
    DY      = ZP_TMP_19 + 1
    X_VAL   = ZP_TMP_20
    Y_VAL   = ZP_TMP_20 + 1
    sta Y_VAL ; y is in A register
    jsr popa
    sta X_VAL 

    lda _picDrawEnabled
    ora _priDrawEnabled
    bne @ok_fill
    jmp pop_done

    ; can_fill X_VAL, Y_VAL
    ; bne @ok_fill
    ; rts
@ok_fill:

    ; fill_stack_pointer = 0;
    stz FILL_STACK_POINTER

    ; scan_and_fill(x, y);
    ldx X_VAL
    ldy Y_VAL
    b8ScanAndFill

    ; while (pop(&lx, &rx, &y1)) {
pop_loop:
    FILL_STACK_POP LX, RX, Y1
    bne @pop_not_done ; bne branches if Z flag is clear (ie not equal to zero)
    jmp pop_done
@pop_not_done:
    ; nx = lx;
    lda LX
    sta NX
    ; while (nx <= rx) {
outer_loop_start:
    lda RX
    ;This instruction subtracts the contents of memory from the contents of the accumulator.
    ;The use of the CMP affects the following flags: 
    ; Z flag is set on an equal comparison, reset otherwise (ie M==A Z=1)
    ; the N flag is set or reset by the result bit 7, (ie M-A<0 N=1)
    ; the carry flag is set when the value in memory is less than or equal to the accumulator, (ie M<=A C=1)
    cmp NX ; compare NX to RX
    bcs @nx_less_than_rx ; bcc branches if C flag is set (ie NX <= RX)
    jmp outer_loop_end
@nx_less_than_rx:
    ; if (can_fill(nx, y1)) {
    can_fill NX, Y1
    cmp #0
    bne @start_fill ; branch if can_fill returned true
    jmp else_increment_nx
    @start_fill:
    ; scan_and_fill(nx, y1);
    ldx NX
    ldy Y1
    b8ScanAndFill
    ; while (nx <= rx && can_fill(nx, y1)) {
inner_loop_start:
    lda NX
    cmp RX
    ; bcs outer_loop_start ; bcs branches if C flag is set (ie NX > RX)
    bcc @nx_less_than_rx_inner
    jmp else_increment_nx
@nx_less_than_rx_inner:
    can_fill NX, Y1
    bne @can_fill_inner
    jmp outer_loop_start
@can_fill_inner:
    ; ++nx;
    inc NX
    jmp inner_loop_start
else_increment_nx:
    ; ++nx;
    inc NX
    jmp outer_loop_start
outer_loop_end:
    jmp pop_loop
pop_done:
    rts
.endproc ; _asm_flood_fill



.endif