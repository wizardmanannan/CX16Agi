.segment "BANKRAM08"
.ifndef  LINE_INC

LINE_INC = 1

.include "lineDrawing.s"

b8LineTableLow: .res PICTURE_HEIGHT
b8LineTableHigh: .res PICTURE_HEIGHT


b8ColorTable:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

.macro ADD_PRIORITY_OFFSET
clc
lda VERA_addr_high
adc #>PRIORITY_START
sta VERA_addr_high
.endmacro

.proc b8SetupLineTable 
 ; each entry is a 16 bit value
    lda #<STARTING_BYTE
    sta b8LineTableLow
    lda #>STARTING_BYTE
    sta b8LineTableHigh
    ldx #0
    ; load the previous value
    lda b8LineTableHigh,x
    tay
    lda b8LineTableLow,x

    inx
@lineTableLoop:
    clc
    adc #<LINE_LENGTH   ; add LINE_LENGTH
    sta b8LineTableLow,x
    tya
    adc #>LINE_LENGTH   ; add carry
    sta b8LineTableHigh,x
    tay                 ; load previous value for next iteration
    lda b8LineTableLow,x
    
    inx
    cpx #168          ; did we reach 168 lines? 
    bne @lineTableLoop
rts
.endproc

.macro PLOT_VIS PIC_COLOUR
ldy PIC_COLOUR
lda b8ColorTable,y
sta VERA_data0         ; Write the color to VRAM

.endmacro

.macro PLOT_PRIORITY PRI_COLOUR
.local @end
stz VERA_ctrl
    stz VERA_addr_bank
    
    clc
    lsr VERA_addr_high
    ror VERA_addr_low
    bcc @evenPriority
    
    @oddPriority:
    ADD_PRIORITY_OFFSET
    lda VERA_data0
    and #$F0
    ora PRI_COLOUR
    sta VERA_data0
    bra @end
    
    @evenPriority:
    ADD_PRIORITY_OFFSET
    lda VERA_data0
    and #$F
    sta VERA_data0
    lda PRI_COLOUR
    asl
    asl
    asl
    asl
    ora VERA_data0
    sta VERA_data0
@end:
.endmacro

.macro CALC_VRAM_ADDR_LINE_DRAW xpos_low, xpos_high, ypos, tmpZP ;Set any value to the last param to disable xTimes2
.scope
    ; set bank to 30 TODO: use rodata or somewhere else?
    ; lda #$30
    ; sta $00

    ; make use of the lookup table
    stz VERA_ctrl
    ldx ypos
    lda b8LineTableLow,x     ; Get the low byte of the address
    sta tmpZP
    lda b8LineTableHigh,x   ; Get the high byte of the address
    sta tmpZP + 1
    
    ; set bank back to 0
    ; stz $00

    ; Calculate (x0 >> 1)
    lda xpos_high
    lsr
    lda xpos_low
    ror                 ; keep result in A

    ; Add (y << 5) + (y << 7) + (x0 >> 1)
    clc
    adc tmpZP
    sta tmpZP
    lda tmpZP + 1
    adc #$00            ; keep result in A

    ; Store the result in the VRAM address register
    sta VERA_addr_high
    lda tmpZP

    sta VERA_addr_low
    stz VERA_addr_bank ; Disable auto-increment, set address bank to 0
.endscope
.endmacro ; CALC_VRAM_ADDR


.macro CALC_VRAM_ADDR_LINE_DRAW_160 xpos, ypos
    ; same as calc_vram_addr without the (x >> 1) part
    vram_addr_l     =  ZP_TMP_21
    vram_addr_h     = ZP_TMP_21 + 1 

    ; set bank to 30
    ; lda #$30
    ; sta $00

    ; make use of the lookup table at $30A000
    ldx ypos
    lda b8LineTableLow,x     ; Get the low byte of the address
    sta vram_addr_l
    lda b8LineTableHigh,x   ; Get the high byte of the address
    sta vram_addr_h

    ; set bank back to 0
    ; stz $00
    
    ; Add vram_addr + x
    lda xpos
    clc
    adc vram_addr_l            ; add low byte of (y << 5) + (y << 7)
    sta vram_addr_l            ; store low byte result (because 160<0xff)
    lda vram_addr_h
    adc #$00                   ; add carry
    sta vram_addr_h            ; store high byte result

    ; Store the result in the VRAM address register
    sta VERA_addr_high
    lda vram_addr_l
    sta VERA_addr_low
    stz VERA_addr_bank ; clear the upper byte of the VRAM address and any auto increment
.endmacro ; calc_vram_addr_160

.macro CALC_VRAM_ADDR XPOS, YPOS, TMP
    ; make use of the lookup table
    stz VERA_ctrl
    clc
    ldx YPOS
  
    lda b8LineTableLow,x     ; Get the low byte of the address
    sta TMP
    lda b8LineTableHigh,x   ; Get the high byte of the address
    sta TMP + 1
        
    
    lda XPOS
    ; Add (y << 5) + (y << 7) + (x0 >> 1)
    clc
    adc TMP
    sta TMP
    lda #$0
    adc TMP + 1           ; keep result in A

    ; Store the result in the VRAM address register
    sta VERA_addr_high
    lda TMP

    sta VERA_addr_low
    stz VERA_addr_bank ; Disable auto-increment, set address bank to 0
.endmacro ; CALC_VRAM_ADDR

.import _picColour, _priColour, _picDrawEnabled, _priDrawEnabled
 X_POS = ZP_TMP_2
 Y_POS = ZP_TMP_2 + 1
 ;void drawVisualPixel(byte x, byte y)
 _b8DrawPixel:
    nop
    nop
    nop
    sta Y_POS
    jsr popa
    sta X_POS

    @visual:
    lda _picDrawEnabled
    beq priority

    CALC_VRAM_ADDR X_POS, Y_POS, ZP_TMP_5

    PLOT_VIS _picColour

    priority:
    lda _priDrawEnabled
    beq @end

    CALC_VRAM_ADDR X_POS, Y_POS, ZP_TMP_5

    PLOT_PRIORITY _priColour

    @end:
    rts

;void b8AsmDrawLine(unsigned short x1, unsigned char y1, unsigned short x2, unsigned char y2)
.export _b8DrawLine
.proc _b8DrawLine
    ; Define temporary storage locations and labels
    Y1_VAL          = ZP_TMP_2
    Y2_VAL          = ZP_TMP_2 + 1
    SX_LOW          = ZP_TMP_5
    SX_HIGH         = ZP_TMP_5 + 1
    SY_LOW          = ZP_TMP_6
    SY_HIGH         = ZP_TMP_6 + 1
    ERR_LOW         = ZP_TMP_7
    ERR_HIGH        = ZP_TMP_7 + 1
    E2_LOW          = ZP_TMP_8 
    E2_HIGH         = ZP_TMP_8 + 1
    X_LOW_TEMP      = ZP_TMP_9
    X_HIGH_TEMP     = ZP_TMP_9 + 1

    X1_LOW          = ZP_TMP_10
    X1_HIGH         = ZP_TMP_10 + 1
    X2_LOW          = ZP_TMP_12
    X2_HIGH         = ZP_TMP_12 + 1
    Y_VAL_TEMP      = ZP_TMP_13
    Y_VAL_TEMP_HIGH      = ZP_TMP_13 + 1
    DX_LOW          = ZP_TMP_14
    DX_HIGH         = ZP_TMP_14 + 1
    DY_LOW          = ZP_TMP_16
    DY_HIGH         = ZP_TMP_16 + 1
    PIC_COLOUR = ZP_TMP_17
    PRI_COLOUR = ZP_TMP_17 + 1
    PIC_DRAW_ENABLED = ZP_TMP_18
    PRI_DRAW_ENABLED = ZP_TMP_18 + 1
start:
    clc
    sta Y2_VAL

    ; Get parameters from the C stack
    jsr popa
    sta X2_LOW
    jsr popa
    sta X2_HIGH
    
    jsr popa
    sta Y1_VAL

    jsr popa
    sta X1_LOW
    jsr popa
    sta X1_HIGH

    lda _picColour
    sta PIC_COLOUR

    lda _priColour
    sta PRI_COLOUR

    lda _picDrawEnabled
    sta PIC_DRAW_ENABLED

    lda _priDrawEnabled
    sta PRI_DRAW_ENABLED

    ; dx = abs(x2 - x1);
    lda X2_LOW
    sec
    sbc X1_LOW
    sta DX_LOW
    lda X2_HIGH
    sbc X1_HIGH
    sta DX_HIGH
    bpl @dx_positive
    ; If it's negative, negate it
    lda #$FF
    sec
    sbc DX_LOW            ; A = 0xFF - (-1) = 0x100
    sta DX_LOW            ; DX_LOW = 0
    lda #$FF
    sbc DX_HIGH           ; A = 0xFF - 0 - 1 (borrow) = 0xFE
    sta DX_HIGH           ; DX_HIGH = 0xFE
    inc DX_LOW            ; DX_LOW = 1, DX_HIGH:DX_LOW = 0x00:0x01
@dx_positive:
    ; dy = -abs(y2 - y1);
    lda Y2_VAL
    sec
    sbc Y1_VAL
    sta DY_LOW
    lda #0
    sbc #0
    sta DY_HIGH
    ; If DY is positive, negate it
    bpl @negate_dy
    jmp @dy_done
@negate_dy:
    lda #0
    sec
    sbc DY_LOW
    sta DY_LOW
    lda #0
    sbc DY_HIGH
    sta DY_HIGH
@dy_done:

    ; sx = x1 < x2 ? 1 : -1;
    lda X1_LOW
    cmp X2_LOW
    lda X1_HIGH
    sbc X2_HIGH
    bvc @x_sign_check
    eor #$80
@x_sign_check:
    bpl @x1_greater_than_equal_x2
    lda #1
    sta SX_LOW
    lda #0
    sta SX_HIGH
    bra @sx_done
@x1_greater_than_equal_x2:
    lda #$FF
    sta SX_LOW
    sta SX_HIGH
@sx_done:

    ; sy = y1 < y2 ? 1 : -1;
    lda Y1_VAL
    cmp Y2_VAL
    bcs @y1_greater_than_equal_y2
    lda #1
    sta SY_LOW
    lda #0
    sta SY_HIGH
    bra @sy_done
@y1_greater_than_equal_y2:
    lda #$FF
    sta SY_LOW
    sta SY_HIGH
@sy_done:

    ; err = dx + dy;
    lda DX_LOW
    clc
    adc DY_LOW
    sta ERR_LOW
    lda DX_HIGH
    adc DY_HIGH
    sta ERR_HIGH

loop_start:
    ; if (vis_enabled)
    ;     asm_plot_vis_pixel((x1 << 1), y1 + STATUSBAR_OFFSET, vis_colour);
    lda PIC_DRAW_ENABLED
    bne @plot_vis  ; Invert the condition
    jmp skip_vis  ; Jump to @skip_vis if PIC_DRAW_ENABLED is zero

@plot_vis:
    lda X1_LOW
    asl
    sta X_LOW_TEMP
    lda X1_HIGH
    rol
    sta X_HIGH_TEMP
    CALC_VRAM_ADDR_LINE_DRAW X_LOW_TEMP, X_HIGH_TEMP, Y1_VAL, Y_VAL_TEMP

    PLOT_VIS PIC_COLOUR

skip_vis:

    ; if (pri_enabled)
    ;     asm_plot_pri_pixel((x1 << 1), y1 + STATUSBAR_OFFSET, pri_colour);
    lda PRI_DRAW_ENABLED
    bne @plot_pri  ; Invert the condition
    jmp skip_pri  ; Jump to @skip_pri if PRI_DRAW_ENABLED is zero

@plot_pri: 
    lda X1_LOW
    asl
    sta X_LOW_TEMP
    lda X1_HIGH
    rol
    sta X_HIGH_TEMP
    CALC_VRAM_ADDR_LINE_DRAW X_LOW_TEMP, X_HIGH_TEMP, Y1_VAL, Y_VAL_TEMP
    
    PLOT_PRIORITY PRI_COLOUR

skip_pri:

    ; if (x1 == x2 && y1 == y2)
    ;     break;     // Check end condition
    lda X1_LOW
    cmp X2_LOW
    bne @continue ; if not equal, continue
    lda X1_HIGH
    cmp X2_HIGH
    bne @continue ; if not equal, continue
    lda Y1_VAL
    cmp Y2_VAL
    bne @continue ; if not equal, continue
    jmp @end_loop ; if all are equal, end loop
@continue:

    ; e2 = 2 * err;
    lda ERR_LOW
    asl
    sta E2_LOW
    lda ERR_HIGH
    rol
    sta E2_HIGH

    ; if (e2 >= dy) {
    ;     err += dy;
    ;     x1 += sx;
    ; }
    ; check if e2 >= dy
    lda E2_HIGH
    cmp DY_HIGH
    bmi @e2_less_than_dy ; do nothing if e2 < dy
    bne @e2_greater_than_dy ; branch if e2 != dy
    lda E2_LOW
    cmp DY_LOW
    bcc @e2_less_than_dy ; it must be greater or equal
@e2_greater_than_dy:
    ; err += dy;
    lda ERR_LOW
    clc
    adc DY_LOW
    sta ERR_LOW
    lda ERR_HIGH
    adc DY_HIGH
    sta ERR_HIGH
    ; x1 += sx;
    lda X1_LOW
    clc
    adc SX_LOW
    sta X1_LOW
    lda X1_HIGH
    adc SX_HIGH
    sta X1_HIGH
@e2_less_than_dy:

    ; if (e2 <= dx) {
    ;     err += dx;
    ;     y1 += sy;
    ; }
    lda E2_HIGH
    cmp DX_HIGH
    bmi @e2_less_than_dx
    bne @e2_greater_than_dx
    lda E2_LOW
    cmp DX_LOW
    bcc @e2_less_than_dx
    beq @e2_less_than_dx
@e2_greater_than_dx:
    jmp loop_start
@e2_less_than_dx:
    lda ERR_LOW
    clc
    adc DX_LOW
    sta ERR_LOW
    lda ERR_HIGH
    adc DX_HIGH
    sta ERR_HIGH
    ; y1 += sy;
    lda Y1_VAL
    ldx SY_HIGH
    bne @negative_slope
@positive_slope:
    clc
    adc #$1
    sta Y1_VAL
    jmp loop_start

@negative_slope:
    sec
    sbc #$1
    sta Y1_VAL
    jmp loop_start

@end_loop:
    rts
.endproc ; _asm_drawline

.endif