.segment "BANKRAM08"
b8LineTable: .res PICTURE_HEIGHT * 2

b8ColorTable:
    .byte $00, $11, $22, $33, $44, $55, $66, $77, $88, $99, $AA, $BB, $CC, $DD, $EE, $FF

.proc b8SetupLineTable 
    ; each entry is a 16 bit value
    stz b8LineTable
    stz b8LineTable+1
    ldx #0
    ; load the previous value
    lda b8LineTable+1,x
    tay
    lda b8LineTable,x
@loop1:
    clc
    inx
    inx
    adc #<LINE_LENGTH   ; add LINE_LENGTH
    sta b8LineTable,x
    tya
    adc #>LINE_LENGTH   ; add carry
    sta b8LineTable+1,x
    tay                 ; load previous value for next iteration
    lda b8LineTable,x
    cpx #PICTURE_HEIGHT - 2          ; did we reach 168 lines? 
    bcc @loop1
    ldx #0
@loop2:
    clc
    inx
    inx
    adc #<LINE_LENGTH   ; add LINE_LENGTH
    sta b8LineTable+PICTURE_HEIGHT - 2,x
    tya
    adc #>LINE_LENGTH   ; add carry
    sta b8LineTable+PICTURE_HEIGHT - 2 + 1,x
    tay                 ; load previous value for next iteration
    lda b8LineTable+PICTURE_HEIGHT - 2,x
    cpx #PICTURE_HEIGHT - 1            ; did we reach 168 lines?
    bcc @loop2
rts
.endproc

.macro calc_vram_addr xpos_low, xpos_high, ypos, tmpZP
.scope
    ; set bank to 30 TODO: use rodata or somewhere else?
    ; lda #$30
    ; sta $00

    ; make use of the lookup table
    stz VERA_ctrl
    clc
    lda ypos
    asl                 ; (y << 1)
    bcc @lower_bound    ; if carry is clear, then the result is less than 256
    tax
    lda b8LineTable+256,x     ; Get the low byte of the address
    sta tmpZP
    lda b8LineTable+256+1,x   ; Get the high byte of the address
    sta tmpZP + 1
    bra @done
    @lower_bound:
    tax
    lda b8LineTable,x         ; Get the low byte of the address
    sta tmpZP
    lda b8LineTable+1,x         ; Get the high byte of the address
    sta tmpZP + 1
    @done:

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
.endmacro ; calc_vram_addr

.import _picColour, _priColour, _picDrawEnabled, _priDrawEnabled

;void b8AsmDrawLine(unsigned short x1, unsigned char y1, unsigned short x2, unsigned char y2)
.export _b8DrawLine
.proc _b8DrawLine
    ; Define temporary storage locations and labels
    Y1_VAL          = ZP_TMP_2
    Y2_VAL          = ZP_TMP_2 + 1
    DX_LOW          = ZP_TMP_14
    DX_HIGH         = ZP_TMP_14 + 1
    DY_LOW          = ZP_TMP_16
    DY_HIGH         = ZP_TMP_16 + 1
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
    lda _picDrawEnabled
    bne @plot_vis  ; Invert the condition
    jmp skip_vis  ; Jump to @skip_vis if _picDrawEnabled is zero

@plot_vis:
    lda X1_LOW
    asl
    sta X_LOW_TEMP
    lda X1_HIGH
    rol
    sta X_HIGH_TEMP
    calc_vram_addr X_LOW_TEMP, X_HIGH_TEMP, Y1_VAL, Y_VAL_TEMP

    clc ;Skip Over the non drawable section
    lda VERA_addr_low
    adc #<STARTING_BYTE
    sta VERA_addr_low
    lda VERA_addr_high
    adc #>STARTING_BYTE
    sta VERA_addr_high

    ldy _picColour
    lda b8ColorTable,y
    sta VERA_data0         ; Write the color to VRAM

skip_vis:

    ; if (pri_enabled)
    ;     asm_plot_pri_pixel((x1 << 1), y1 + STATUSBAR_OFFSET, pri_colour);
    lda _priDrawEnabled
    bne @plot_pri  ; Invert the condition
    jmp skip_pri  ; Jump to @skip_pri if _priDrawEnabled is zero

@plot_pri:
    lda X1_LOW
    asl
    sta X_LOW_TEMP
    lda X1_HIGH
    rol
    sta X_HIGH_TEMP
    calc_vram_addr X_LOW_TEMP, X_HIGH_TEMP, Y1_VAL, Y_VAL_TEMP
    ; Add 0x9800 to the VERA::ADDR
    lda VERA_addr_high
    clc
    adc #$80
    stz VERA_ctrl
    sta VERA_addr_high
    lda VERA_addr_bank
    adc #$00
    and #%00000001            ; Throw away any other bits
    sta VERA_addr_bank
    ldy _priColour
    lda b8ColorTable,y
    ;sta VERA_data0         ; Write the color to VRAM

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