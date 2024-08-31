.segment "BANKRAM08"
.ifndef  LINE_INC

LINE_INC = 1

.include "lineDrawing.s"

b8LineTableVisualLow: .res PICTURE_HEIGHT
b8LineTableVisualHigh: .res PICTURE_HEIGHT

b8LineTablePriorityLow: .res PICTURE_HEIGHT
b8LineTablePriorityHigh: .res PICTURE_HEIGHT


b8ColorTable:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

;starting byte a/x
TABLE_LOW = ZP_TMP_2
TABLE_HIGH = ZP_TMP_3
LINE_LENGTH_ZP = ZP_TMP_4
.proc b8SetupLineTable
 ; each entry is a 16 bit value   
    ldy #$0
    
    sta (TABLE_LOW),y
    pha
    txa
    sta (TABLE_HIGH),y
    pla
 
    iny
@lineTableLoop:
    clc
    adc LINE_LENGTH_ZP   ; add LINE_LENGTH
    sta (TABLE_LOW),y
    pha
    txa
    adc #$0 ; add carry
    sta (TABLE_HIGH),y
    tax                 ; load previous value for next iteration
    pla
    
    iny
    cpy #PICTURE_HEIGHT         ; did we reach 168 lines? 
    bne @lineTableLoop
rts
.endproc

.proc b8SetupLineTables
;Setup Visual
lda #< b8LineTableVisualLow
sta TABLE_LOW
lda #> b8LineTableVisualLow
sta TABLE_LOW + 1

;Setup Visual
lda #< b8LineTableVisualHigh
sta TABLE_HIGH
lda #> b8LineTableVisualHigh
sta TABLE_HIGH + 1

lda #LINE_LENGTH
sta LINE_LENGTH_ZP

lda #<STARTING_BYTE
ldx #>STARTING_BYTE
jsr b8SetupLineTable

;Setup Priority
lda #< b8LineTablePriorityLow
sta TABLE_LOW
lda #> b8LineTablePriorityLow
sta TABLE_LOW + 1

lda #< b8LineTablePriorityHigh
sta TABLE_HIGH
lda #> b8LineTablePriorityHigh
sta TABLE_HIGH + 1

lda #LINE_LENGTH / 2
sta LINE_LENGTH_ZP

lda #<PRIORITY_START
ldx #>PRIORITY_START
jsr b8SetupLineTable

rts
.endproc

.macro PLOT_VIS PIC_COLOUR
ldy PIC_COLOUR
lda b8ColorTable,y
sta VERA_data0         ; Write the color to VRAM

.endmacro

.macro PLOT_PRIORITY PRI_COLOUR, X_POS
.local @end
stz VERA_ctrl
    stz VERA_addr_bank
    
    lda X_POS
    lsr 
    bcc @evenPriority
    
    @oddPriority:
    lda VERA_data0
    and #$F0
    ora PRI_COLOUR
    sta VERA_data0
    bra @end
    
    @evenPriority:
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

.macro CALC_VRAM_ADDR_LINE_DRAW xpos_low, ypos, tmpZP ;Set any value to the last param to disable xTimes2
.scope
    ; set bank to 30 TODO: use rodata or somewhere else?
    ; lda #$30
    ; sta $00

    stz VERA_ctrl
        ; set bank back to 0
    ; stz $00

    lda xpos_low
    ror                 ; keep result in A

    ; make use of the lookup table
    ; Add (y << 5) + (y << 7) + (x0 >> 1)
    ldx ypos
    clc
    adc b8LineTableVisualLow,x   ; Get the high byte of the address
    sta VERA_addr_low
    lda b8LineTableVisualHigh,x   ; Get the high byte of the address
    adc #$00            ; keep result in A
    ; Store the result in the VRAM address register
    sta VERA_addr_high

    stz VERA_addr_bank ; Disable auto-increment, set address bank to 0
.endscope
.endmacro ; CALC_VRAM_ADDR


.macro CALC_VRAM_ADDR_PRI_LINE_DRAW xpos_low, ypos, tmpZP ;Set any value to the last param to disable xTimes2
.scope
    ; set bank to 30 TODO: use rodata or somewhere else?
    ; lda #$30
    ; sta $00
    
    stz VERA_ctrl
    
    lda xpos_low
    lsr                 ; keep result in A

    ; Add (y << 5) + (y << 7) + (x0 >> 1)
    ; make use of the lookup table
    ldx ypos
    clc
    adc b8LineTablePriorityLow,x  
    sta VERA_addr_low
    lda b8LineTablePriorityHigh,x
    adc #$00            ; keep result in A
    ; Store the result in the VRAM address register
    sta VERA_addr_high

    stz VERA_addr_bank ; Disable auto-increment, set address bank to 0

.endscope
.endmacro ; CALC_VRAM_ADDR

.macro VERA_CTRL_SET vera_ctrl_value
.ifnblank vera_ctrl_value
        lda vera_ctrl_value
        .if (.xmatch (#0, vera_ctrl_value))
            stz VERA_ctrl
        .else 
            lda vera_ctrl_value
            sta VERA_ctrl
        .endif
    .endif
.endmacro

.macro CALC_VRAM_ADDR_VISUAL xpos, vera_ctrl_value
    
    VERA_CTRL_SET vera_ctrl_value

    lda xpos
    clc
    adc b8LineTableVisualLow,y             ; add low byte of (y << 5) + (y << 7)
    sta VERA_addr_low         ; store low byte result (because 160<0xff)
    lda b8LineTableVisualHigh,y 
    adc #$00                   ; add carry
    sta VERA_addr_high
    stz VERA_addr_bank ; clear the upper byte of the VRAM address and any auto increment
.endmacro ; calc_vram_addr_160

.macro CALC_VRAM_ADDR_PRIORITY xpos, vera_ctrl_value    
    
    VERA_CTRL_SET vera_ctrl_value


    lda xpos
    lsr

    clc
    adc b8LineTablePriorityLow,y             ; add low byte of (y << 5) + (y << 7)
    sta VERA_addr_low         ; store low byte result (because 160<0xff)
    lda b8LineTablePriorityHigh,y 
    adc #$00                   ; add carry
    sta VERA_addr_high
    stz VERA_addr_bank ; clear the upper byte of the VRAM address and any auto increment
.endmacro ; calc_vram_addr_160

.import _picColour, _priColour, _picDrawEnabled, _priDrawEnabled
 X_POS = ZP_TMP_2
 Y_POS = ZP_TMP_2 + 1
 ;void drawVisualPixel(byte x, byte y)
 _b8DrawPixel:
    sta Y_POS
    jsr popa
    sta X_POS

    @visual:
    lda _picDrawEnabled
    beq priority

    ldy Y_POS
    CALC_VRAM_ADDR_VISUAL X_POS, #$0

    PLOT_VIS _picColour

    priority:
    lda _priDrawEnabled
    beq @end

    ldy Y_POS
    CALC_VRAM_ADDR_PRIORITY X_POS, #$0

    PLOT_PRIORITY _priColour, X_POS

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

    X1_LOW          = ZP_TMP_10
    X1_HIGH         = ZP_TMP_10 + 1
    X2_LOW          = ZP_TMP_12
    X2_HIGH         = ZP_TMP_12 + 1
    Y_VAL_TEMP      = ZP_TMP_19 + 1
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
    CALC_VRAM_ADDR_LINE_DRAW X_LOW_TEMP, Y1_VAL, Y_VAL_TEMP

    PLOT_VIS PIC_COLOUR

skip_vis:

    ; if (pri_enabled)
    ;     asm_plot_pri_pixel((x1 << 1), y1 + STATUSBAR_OFFSET, pri_colour);
    lda PRI_DRAW_ENABLED
    bne @plot_pri  ; Invert the condition
    jmp skip_pri  ; Jump to @skip_pri if PRI_DRAW_ENABLED is zero

@plot_pri: 
    CALC_VRAM_ADDR_PRI_LINE_DRAW X1_LOW, Y1_VAL, Y_VAL_TEMP

    PLOT_PRIORITY PRI_COLOUR, X1_LOW

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