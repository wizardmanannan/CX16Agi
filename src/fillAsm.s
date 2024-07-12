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

; .export _asm_plot_vis_hline_fast
.proc _asm_plot_vis_hline_fast
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
    ;;jsr asm_plot_vis_hline
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
    CALC_VRAM_ADDR_LINE_DRAW X0_LOW, X0_HIGH, Y0, TMP

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

.endif