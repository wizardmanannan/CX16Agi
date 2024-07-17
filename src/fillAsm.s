.segment "BANKRAM08"

.ifndef  FILL_INC

FILL_INC = 1

.include "lineDrawing.s"

color_table:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

mask_table:
    .byte %11111111 ; 0 $FF
    .byte %11111111 ; 1 $FD
    .byte %11111100 ; 2 $FC
    .byte %11111100 ; 3 $F4
    .byte %11110000 ; 4 $F0
    .byte %11110000 ; 5 $D0
    .byte %11000000 ; 6 $C0
    .byte %11000000 ; 7 $40


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

    clc
    lda #<STARTING_BYTE
    adc VERA_addr_low
    sta VERA_addr_low
    lda #>STARTING_BYTE
    adc VERA_addr_high
    sta VERA_addr_high

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

; asm_plot_vis_hline(unsigned short x0, unsigned short x1, unsigned char y, unsigned char color);
; plots 2 pixels at a time for 160x200 mode
.proc b8AsmPlotVisHLine
    color           = ZP_TMP_10
    Y0              = ZP_TMP_10 + 1
    X1_LOW          = ZP_TMP_12
    X1_HIGH         = ZP_TMP_12 + 1
    X0_LOW          = ZP_TMP_13
    X0_HIGH         = ZP_TMP_13 + 1
    TMP = ZP_TMP_13
    
    ; color is in A register
    sta color

    pop_c_stack Y0
    pop_c_stack X1_LOW
    pop_c_stack X1_HIGH
    pop_c_stack X0_LOW
    pop_c_stack X0_HIGH

    lda X0_LOW
    lda X1_LOW
    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_LOW, Y0
    clc
    lda #<STARTING_BYTE
    adc VERA_addr_low
    sta VERA_addr_low
    lda #>STARTING_BYTE
    adc VERA_addr_high
    sta VERA_addr_high
    
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

    rts                     ; Return from subroutine
.endproc ; _plot_vis_hline

.proc _b8AsmPlotVisHLineFast
    temp            = ZP_TMP_2
    color           = ZP_TMP_2 + 1
    start_mask      = ZP_TMP_5
    end_mask        = ZP_TMP_5 + 1
    length_low      = ZP_TMP_6
    length_high     = ZP_TMP_6 + 1
    Y0              = ZP_TMP_7
    X1_LOW          = ZP_TMP_7 + 1
    X1_HIGH         = ZP_TMP_8
    X0_LOW          = ZP_TMP_8 + 1
    X0_HIGH         = ZP_TMP_9
    TMP = ZP_TMP_9 + 1
    
    ; color is in A register
    sta color

    pop_c_stack Y0
    pop_c_stack X1_LOW
    pop_c_stack X1_HIGH
    pop_c_stack X0_LOW
    pop_c_stack X0_HIGH
    
    ; Calculate the line length and the loop count
    ; Ensure X1 >= X0 
    lda X1_LOW
    sec
    sbc X0_LOW
    sta length_low
    lda X1_HIGH
    sbc X0_HIGH
    sta length_high

    ; Is length less than or equal to 16?
    bne long_line ; length_high is in A
    lda length_low
    clc
    cmp #$10
    bcs long_line

    ; Line is a short line
    lda #%00000100  ; DCSEL = Mode 2 
    sta VERA_ctrl
    stz VERA_dc_video ; Disable cache writing
    ; subtract 5 bytes from c_stack_addr
    lda C_STACK_ADDR
    sec
    sbc #5
    sta C_STACK_ADDR
    lda C_STACK_ADDR + 1
    sbc #0
    sta C_STACK_ADDR + 1
    lda color
    jsr b8AsmPlotVisHLine
    rts ; Return from subroutine

long_line:
    ; Change DCSEL to mode 6 for cache write operations
    lda #%00001100
    sta VERA_ctrl

    ldx color
    lda color_table, x
    sta $9f29
    sta $9f2A
    sta $9f2B
    sta $9f2C

    ; Set up VERA for cache operations
    lda #%00000100  ; DCSEL = Mode 2 for enabling cache
    sta VERA_ctrl
    lda #%01000000  ; Enable cache writing
    sta VERA_dc_video

    ; Calculate the mask for ending chunk
    lda X1_LOW
    and #$7
    tax
    lda mask_table, x
    sta end_mask

    ; Calculate the mask for starting chunk
    lda X0_LOW
    and #$7
    tax
    lda mask_table, x
    eor #$FF ; Invert the mask
    sta start_mask  

    ; Add pixels from start mask to length
    txa ; start length from above
    clc
    adc length_low
    sta length_low
    lda length_high
    adc #0
    sta length_high

    ; Calculate the number of full 8-pixel (32-bit) chunks by dividing by 8 (shift right 3 times)
    lsr length_high   ; Shift right, dividing the high byte by 2
    ror length_low    ; Rotate right the low byte through carry
    lsr length_high   ; Shift right again, further dividing the high byte
    ror length_low    ; Rotate right again
    lsr length_high   ; Final shift right
    ror length_low    ; Final rotate right

    ; Subtract 1 from length
    dec length_low ; length is never higher than 40

    ; *** call the vram address calculation routine ***
    CALC_VRAM_ADDR_LINE_DRAW_160 X0_LOW, Y0
    clc
    lda #<STARTING_BYTE
    adc VERA_addr_low
    sta VERA_addr_low
    lda #>STARTING_BYTE
    adc VERA_addr_high
    sta VERA_addr_high
    

    ; Set address auto-increment to 4 bytes
    lda #%00110000
    sta VERA_addr_bank

    ; draw the starting chunk
    lda start_mask
    sta VERA_data0

    ; Loop counter
    lda #0 ; clear the mask
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

.import _b8Pop
.import _b8ScanAndFill
.import _b8FillStackPointer
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

    ; can_fill X_VAL, Y_VAL
    ; bne @ok_fill
    ; rts
@ok_fill:

    ; fill_stack_pointer = 0;
    stz _b8FillStackPointer

    ; scan_and_fill(x, y);
    lda X_VAL
    jsr pusha
    lda Y_VAL
    jsr _b8ScanAndFill

    ; while (pop(&lx, &rx, &y1)) {
pop_loop:
    ldx #$00
    lda #LX
    jsr pushax
    lda #RX
    jsr pushax
    lda #Y1
    jsr _b8Pop
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
    lda NX
    jsr pusha
    lda Y1
    jsr _b8ScanAndFill
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